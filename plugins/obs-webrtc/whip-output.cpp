#include "whip-output.h"
#include "whip-utils.h"

/*
 * Sets the maximum size for a video fragment. Effective range is
 * 576-1470, with a lower value equating to more packets created,
 * but also better network compatability.
 */
static uint16_t MAX_VIDEO_FRAGMENT_SIZE = 1200;

const int signaling_media_id_length = 16;
const char signaling_media_id_valid_char[] = "0123456789"
					     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					     "abcdefghijklmnopqrstuvwxyz";

const std::string user_agent = generate_user_agent();

const char *audio_mid = "0";
const uint32_t audio_clockrate = 48000;
const uint8_t audio_payload_type = 111;

const char *video_mid = "1";
const uint32_t video_clockrate = 90000;
const uint8_t video_payload_type = 96;

WHIPOutput::WHIPOutput(obs_data_t *, obs_output_t *output)
	: output(output),
	  endpoint_url(),
	  bearer_token(),
	  resource_url(),
	  running(false),
	  start_stop_mutex(),
	  start_stop_thread(),
	  base_ssrc(generate_random_u32()),
	  peer_connection(-1),
	  audio_track(-1),
	  video_track(-1),
	  total_bytes_sent(0),
	  connect_time_ms(0),
	  start_time_ns(0),
	  last_audio_timestamp(0),
	  last_video_timestamp(0)
{
}

WHIPOutput::~WHIPOutput()
{
	Stop();

	std::lock_guard<std::mutex> l(start_stop_mutex);
	if (start_stop_thread.joinable())
		start_stop_thread.join();
}

bool WHIPOutput::Start()
{
	std::lock_guard<std::mutex> l(start_stop_mutex);

	if (!obs_output_can_begin_data_capture(output, 0))
		return false;
	if (!obs_output_initialize_encoders(output, 0))
		return false;

	if (start_stop_thread.joinable())
		start_stop_thread.join();
	start_stop_thread = std::thread(&WHIPOutput::StartThread, this);

	return true;
}

void WHIPOutput::Stop(bool signal)
{
	std::lock_guard<std::mutex> l(start_stop_mutex);
	if (start_stop_thread.joinable())
		start_stop_thread.join();

	start_stop_thread = std::thread(&WHIPOutput::StopThread, this, signal);
}

void WHIPOutput::Data(struct encoder_packet *packet)
{
	if (!packet) {
		Stop(false);
		obs_output_signal_stop(output, OBS_OUTPUT_ENCODE_ERROR);
		return;
	}

	if (packet->type == OBS_ENCODER_AUDIO) {
		int64_t duration = packet->dts_usec - last_audio_timestamp;
		Send(packet->data, packet->size, duration, audio_track);
		last_audio_timestamp = packet->dts_usec;
	} else if (packet->type == OBS_ENCODER_VIDEO) {
		int64_t duration = packet->dts_usec - last_video_timestamp;
		Send(packet->data, packet->size, duration, video_track);
		last_video_timestamp = packet->dts_usec;
	}
}

void WHIPOutput::ConfigureAudioTrack(std::string media_stream_id,
				     std::string cname)
{
	auto media_stream_track_id = std::string(media_stream_id + "-audio");

	uint32_t ssrc = base_ssrc;

	rtcTrackInit track_init = {
		RTC_DIRECTION_SENDONLY,
		RTC_CODEC_OPUS,
		audio_payload_type,
		ssrc,
		audio_mid,
		cname.c_str(),
		media_stream_id.c_str(),
		media_stream_track_id.c_str(),
	};

	// Generate a random starting timestamp for the audio track
	uint32_t rtp_audio_timestamp = generate_random_u32();

	rtcPacketizationHandlerInit packetizer_init = {ssrc,
						       cname.c_str(),
						       audio_payload_type,
						       audio_clockrate,
						       0,
						       rtp_audio_timestamp,
						       RTC_NAL_SEPARATOR_LENGTH,
						       0};

	audio_track = rtcAddTrackEx(peer_connection, &track_init);
	rtcSetOpusPacketizationHandler(audio_track, &packetizer_init);
	rtcChainRtcpSrReporter(audio_track);
	rtcChainRtcpNackResponder(audio_track, 1000);
}

void WHIPOutput::ConfigureVideoTrack(std::string media_stream_id,
				     std::string cname)
{
	auto media_stream_track_id = std::string(media_stream_id + "-video");

	// More predictable SSRC values between audio and video
	uint32_t ssrc = base_ssrc + 1;

	rtcTrackInit track_init = {
		RTC_DIRECTION_SENDONLY,
		RTC_CODEC_H264,
		video_payload_type,
		ssrc,
		video_mid,
		cname.c_str(),
		media_stream_id.c_str(),
		media_stream_track_id.c_str(),
	};

	// Generate a random starting timestamp for the video track
	uint32_t rtp_video_timestamp = generate_random_u32();

	rtcPacketizationHandlerInit packetizer_init = {
		ssrc,
		cname.c_str(),
		video_payload_type,
		video_clockrate,
		0,
		rtp_video_timestamp,
		RTC_NAL_SEPARATOR_START_SEQUENCE,
		MAX_VIDEO_FRAGMENT_SIZE};

	video_track = rtcAddTrackEx(peer_connection, &track_init);
	rtcSetH264PacketizationHandler(video_track, &packetizer_init);
	rtcChainRtcpSrReporter(video_track);
	rtcChainRtcpNackResponder(video_track, 1000);
}

/**
 * @brief Initialize encoders and store connect info provided by the service.
 *
 * @return bool
 */
bool WHIPOutput::Init()
{
	if (!obs_output_can_begin_data_capture(output, 0))
		return false;

	if (!obs_output_initialize_encoders(output, 0))
		return false;

	obs_service_t *service = obs_output_get_service(output);
	if (!service) {
		obs_output_signal_stop(output, OBS_OUTPUT_ERROR);
		return false;
	}

	endpoint_url = obs_service_get_connect_info(
		service, OBS_SERVICE_CONNECT_INFO_SERVER_URL);
	if (endpoint_url.empty()) {
		obs_output_signal_stop(output, OBS_OUTPUT_BAD_PATH);
		return false;
	}

	bearer_token = obs_service_get_connect_info(
		service, OBS_SERVICE_CONNECT_INFO_BEARER_TOKEN);

	return true;
}

/**
 * @brief Set up the PeerConnection and media tracks.
 *
 * @return bool
 */
bool WHIPOutput::Setup()
{
	rtcConfiguration config;
	memset(&config, 0, sizeof(config));

	peer_connection = rtcCreatePeerConnection(&config);
	rtcSetUserPointer(peer_connection, this);

	rtcSetStateChangeCallback(peer_connection, [](int, rtcState state,
						      void *ptr) {
		auto whipOutput = static_cast<WHIPOutput *>(ptr);
		switch (state) {
		case RTC_NEW:
			do_log_s(LOG_INFO, "PeerConnection state is now: New");
			break;
		case RTC_CONNECTING:
			do_log_s(LOG_INFO,
				 "PeerConnection state is now: Connecting");
			whipOutput->start_time_ns = os_gettime_ns();
			break;
		case RTC_CONNECTED:
			do_log_s(LOG_INFO,
				 "PeerConnection state is now: Connected");
			whipOutput->connect_time_ms =
				(int)((os_gettime_ns() -
				       whipOutput->start_time_ns) /
				      1000000.0);
			do_log_s(LOG_INFO, "Connect time: %dms",
				 whipOutput->connect_time_ms.load());
			break;
		case RTC_DISCONNECTED:
			do_log_s(LOG_INFO,
				 "PeerConnection state is now: Disconnected");
			whipOutput->Stop(false);
			obs_output_signal_stop(whipOutput->output,
					       OBS_OUTPUT_DISCONNECTED);
			break;
		case RTC_FAILED:
			do_log_s(LOG_INFO,
				 "PeerConnection state is now: Failed");
			whipOutput->Stop(false);
			obs_output_signal_stop(whipOutput->output,
					       OBS_OUTPUT_ERROR);
			break;
		case RTC_CLOSED:
			do_log_s(LOG_INFO,
				 "PeerConnection state is now: Closed");
			break;
		}
	});

	std::string media_stream_id, cname;
	media_stream_id.reserve(signaling_media_id_length);
	cname.reserve(signaling_media_id_length);

	for (int i = 0; i < signaling_media_id_length; ++i) {
		media_stream_id += signaling_media_id_valid_char
			[rand() % (sizeof(signaling_media_id_valid_char) - 1)];

		cname += signaling_media_id_valid_char
			[rand() % (sizeof(signaling_media_id_valid_char) - 1)];
	}

	ConfigureAudioTrack(media_stream_id, cname);
	ConfigureVideoTrack(media_stream_id, cname);

	rtcSetLocalDescription(peer_connection, "offer");

	return true;
}

bool WHIPOutput::Connect()
{
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/sdp");
	if (!bearer_token.empty()) {
		auto bearer_token_header =
			std::string("Authorization: Bearer ") + bearer_token;
		headers =
			curl_slist_append(headers, bearer_token_header.c_str());
	}

	std::string read_buffer;
	std::vector<std::string> location_headers;

	char offer_sdp[4096] = {0};
	rtcGetLocalDescription(peer_connection, offer_sdp, sizeof(offer_sdp));

#ifdef DEBUG_SDP
	do_log(LOG_DEBUG, "Offer SDP:\n%s", offer_sdp);
#endif

	// Add user-agent to our requests
	headers = curl_slist_append(headers, user_agent.c_str());

	CURL *c = curl_easy_init();
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_writefunction);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)&read_buffer);
	curl_easy_setopt(c, CURLOPT_HEADERFUNCTION,
			 curl_header_location_function);
	curl_easy_setopt(c, CURLOPT_HEADERDATA, (void *)&location_headers);
	curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(c, CURLOPT_URL, endpoint_url.c_str());
	curl_easy_setopt(c, CURLOPT_POST, 1L);
	curl_easy_setopt(c, CURLOPT_COPYPOSTFIELDS, offer_sdp);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 8L);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_UNRESTRICTED_AUTH, 1L);

	auto cleanup = [&]() {
		curl_easy_cleanup(c);
		curl_slist_free_all(headers);
	};

	CURLcode res = curl_easy_perform(c);
	if (res != CURLE_OK) {
		do_log(LOG_ERROR,
		       "Connect failed: CURL returned result not CURLE_OK");
		cleanup();
		obs_output_signal_stop(output, OBS_OUTPUT_CONNECT_FAILED);
		return false;
	}

	long response_code;
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &response_code);
	if (response_code != 201) {
		do_log(LOG_ERROR,
		       "Connect failed: HTTP endpoint returned response code %ld",
		       response_code);
		cleanup();
		obs_output_signal_stop(output, OBS_OUTPUT_INVALID_STREAM);
		return false;
	}

	if (read_buffer.empty()) {
		do_log(LOG_ERROR,
		       "Connect failed: No data returned from HTTP endpoint request");
		cleanup();
		obs_output_signal_stop(output, OBS_OUTPUT_CONNECT_FAILED);
		return false;
	}

	long redirect_count = 0;
	curl_easy_getinfo(c, CURLINFO_REDIRECT_COUNT, &redirect_count);

	if (location_headers.size() < static_cast<size_t>(redirect_count) + 1) {
		do_log(LOG_ERROR,
		       "WHIP server did not provide a resource URL via the Location header");
		cleanup();
		obs_output_signal_stop(output, OBS_OUTPUT_CONNECT_FAILED);
		return false;
	}

	CURLU *url_builder = curl_url();
	auto last_location_header = location_headers.back();

	// If Location header doesn't start with `http` it is a relative URL.
	// Construct a absolute URL using the host of the effective URL
	if (last_location_header.find("http") != 0) {
		char *effective_url = nullptr;
		curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &effective_url);
		if (effective_url == nullptr) {
			do_log(LOG_ERROR, "Failed to build Resource URL");
			cleanup();
			obs_output_signal_stop(output,
					       OBS_OUTPUT_CONNECT_FAILED);
			return false;
		}

		curl_url_set(url_builder, CURLUPART_URL, effective_url, 0);
		curl_url_set(url_builder, CURLUPART_PATH,
			     last_location_header.c_str(), 0);
		curl_url_set(url_builder, CURLUPART_QUERY, "", 0);
	} else {
		curl_url_set(url_builder, CURLUPART_URL,
			     last_location_header.c_str(), 0);
	}

	char *url = nullptr;
	CURLUcode rc = curl_url_get(url_builder, CURLUPART_URL, &url,
				    CURLU_NO_DEFAULT_PORT);
	if (rc) {
		do_log(LOG_ERROR,
		       "WHIP server provided a invalid resource URL via the Location header");
		cleanup();
		obs_output_signal_stop(output, OBS_OUTPUT_CONNECT_FAILED);
		return false;
	}

	resource_url = url;
	curl_free(url);
	do_log(LOG_DEBUG, "WHIP Resource URL is: %s", resource_url.c_str());
	curl_url_cleanup(url_builder);

#ifdef DEBUG_SDP
	do_log(LOG_DEBUG, "Answer SDP:\n%s", read_buffer.c_str());
#endif

	rtcSetRemoteDescription(peer_connection, read_buffer.c_str(), "answer");
	cleanup();
	return true;
}

void WHIPOutput::StartThread()
{
	if (!Init())
		return;

	if (!Setup())
		return;

	if (!Connect()) {
		rtcDeletePeerConnection(peer_connection);
		peer_connection = -1;
		audio_track = -1;
		video_track = -1;
		return;
	}

	obs_output_begin_data_capture(output, 0);
	running = true;
}

void WHIPOutput::SendDelete()
{
	if (resource_url.empty()) {
		do_log(LOG_DEBUG,
		       "No resource URL available, not sending DELETE");
		return;
	}

	struct curl_slist *headers = NULL;
	if (!bearer_token.empty()) {
		auto bearer_token_header =
			std::string("Authorization: Bearer ") + bearer_token;
		headers =
			curl_slist_append(headers, bearer_token_header.c_str());
	}

	// Add user-agent to our requests
	headers = curl_slist_append(headers, user_agent.c_str());

	CURL *c = curl_easy_init();
	curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(c, CURLOPT_URL, resource_url.c_str());
	curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, "DELETE");
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 8L);

	auto cleanup = [&]() {
		curl_easy_cleanup(c);
		curl_slist_free_all(headers);
	};

	CURLcode res = curl_easy_perform(c);
	if (res != CURLE_OK) {
		do_log(LOG_WARNING,
		       "DELETE request for resource URL failed. Reason: %s",
		       curl_easy_strerror(res));
		cleanup();
		return;
	}

	long response_code;
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &response_code);
	if (response_code != 200) {
		do_log(LOG_WARNING,
		       "DELETE request for resource URL failed. HTTP Code: %ld",
		       response_code);
		cleanup();
		return;
	}

	do_log(LOG_DEBUG,
	       "Successfully performed DELETE request for resource URL");
	resource_url.clear();
	cleanup();
}

void WHIPOutput::StopThread(bool signal)
{
	if (peer_connection != -1) {
		rtcDeletePeerConnection(peer_connection);
		peer_connection = -1;
		audio_track = -1;
		video_track = -1;
	}

	SendDelete();

	/*
	 * "signal" exists because we have to preserve the "running" state
	 * across reconnect attempts. If we don't emit a signal if
	 * something calls obs_output_stop() and it's reconnecting, you'll
	 * desync the UI, as the output will be "stopped" and not
	 * "reconnecting", but the "stop" signal will have never been
	 * emitted.
	 */
	if (running && signal) {
		obs_output_signal_stop(output, OBS_OUTPUT_SUCCESS);
		running = false;
	}

	total_bytes_sent = 0;
	connect_time_ms = 0;
	start_time_ns = 0;
	last_audio_timestamp = 0;
	last_video_timestamp = 0;
}

void WHIPOutput::Send(void *data, uintptr_t size, uint64_t duration, int track)
{
	if (!running)
		return;

	// Sample time is in microseconds, we need to convert it to seconds
	auto elapsed_seconds = double(duration) / (1000.0 * 1000.0);

	// Get elapsed time in clock rate
	uint32_t elapsed_timestamp = 0;
	rtcTransformSecondsToTimestamp(track, elapsed_seconds,
				       &elapsed_timestamp);

	// Set new timestamp
	uint32_t current_timestamp = 0;
	rtcGetCurrentTrackTimestamp(track, &current_timestamp);
	rtcSetTrackRtpTimestamp(track, current_timestamp + elapsed_timestamp);

	total_bytes_sent += size;
	rtcSendMessage(track, reinterpret_cast<const char *>(data), (int)size);
}

void register_whip_output()
{
	struct obs_output_info info = {};

	info.id = "whip_output";
	info.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_SERVICE;
	info.get_name = [](void *) -> const char * {
		return obs_module_text("Output.Name");
	};
	info.create = [](obs_data_t *settings, obs_output_t *output) -> void * {
		return new WHIPOutput(settings, output);
	};
	info.destroy = [](void *priv_data) {
		delete static_cast<WHIPOutput *>(priv_data);
	};
	info.start = [](void *priv_data) -> bool {
		return static_cast<WHIPOutput *>(priv_data)->Start();
	};
	info.stop = [](void *priv_data, uint64_t) {
		static_cast<WHIPOutput *>(priv_data)->Stop();
	};
	info.encoded_packet = [](void *priv_data,
				 struct encoder_packet *packet) {
		static_cast<WHIPOutput *>(priv_data)->Data(packet);
	};
	info.get_defaults = [](obs_data_t *) {
	};
	info.get_properties = [](void *) -> obs_properties_t * {
		return obs_properties_create();
	};
	info.get_total_bytes = [](void *priv_data) -> uint64_t {
		return (uint64_t) static_cast<WHIPOutput *>(priv_data)
			->GetTotalBytes();
	};
	info.get_connect_time_ms = [](void *priv_data) -> int {
		return static_cast<WHIPOutput *>(priv_data)->GetConnectTime();
	};
	info.encoded_video_codecs = "h264";
	info.encoded_audio_codecs = "opus";
	info.protocols = "WHIP";

	obs_register_output(&info);
}
