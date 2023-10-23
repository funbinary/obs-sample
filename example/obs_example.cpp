#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "obs_ctl.h"
using namespace std;

int main()
{
    {
        int count = 0;
        ObsCtl obsctl;
        obsctl.loadVideoScene();
        //    obsctl.searchVideoTarget();
        obsctl.start();
        while (count < 50)
        {
            count++;
            cout << count << endl;
            std::this_thread::sleep_for(1s);
        }
    }
    std::this_thread::sleep_for(2s);
}
