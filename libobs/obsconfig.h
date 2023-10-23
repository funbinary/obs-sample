#pragma once

/* #undef OBS_DATA_PATH */
/* #undef OBS_PLUGIN_PATH */
/* #undef OBS_PLUGIN_DESTINATION */

/* #undef GIO_FOUND */
/* #undef PULSEAUDIO_FOUND */
/* #undef XCB_XINPUT_FOUND */
/* #undef ENABLE_WAYLAND */

/* NOTE: Release candidate version numbers internally are always the previous
 * main release number!  For example, if the current public release is 21.0 and
 * the build is 22.0 release candidate 1, internally the build number (defined
 * by LIBOBS_API_VER/etc) will always be 21.0, despite the OBS_VERSION string
 * saying "22.0 RC1".
 *
 * If the release candidate version number is 0.0.0 and the RC number is 0,
 * that means it's not a release candidate build. */
#define OBS_RELEASE_CANDIDATE_MAJOR 
#define OBS_RELEASE_CANDIDATE_MINOR 
#define OBS_RELEASE_CANDIDATE_PATCH 
#define OBS_RELEASE_CANDIDATE_VER \
	MAKE_SEMANTIC_VERSION(OBS_RELEASE_CANDIDATE_MAJOR, \
	                      OBS_RELEASE_CANDIDATE_MINOR, \
	                      OBS_RELEASE_CANDIDATE_PATCH)
#define OBS_RELEASE_CANDIDATE 

/* Same thing for beta builds */
#define OBS_BETA_MAJOR 
#define OBS_BETA_MINOR 
#define OBS_BETA_PATCH 
#define OBS_BETA_VER \
	MAKE_SEMANTIC_VERSION(OBS_BETA_MAJOR, \
	                      OBS_BETA_MINOR, \
	                      OBS_BETA_PATCH)
#define OBS_BETA 
