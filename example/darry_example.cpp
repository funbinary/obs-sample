#include <iostream>

#include "util/darray.h"

using namespace std;

int main()
{
    DARRAY(int) array = {0};

    // add
    auto item = da_push_back_new(array);
    *item = 1;

    // find
    auto idx = 0;
    darray_find(array.num, array, 1, idx);

    //remove
    //    for (size_t i = 0; i < core_module_paths.num; ++i)
    //    {
    //        int result = dstr_cmp(&core_module_paths.array[i], path);
    //
    //        if (result == 0)
    //        {
    //            dstr_free(&core_module_paths.array[i]);
    //            da_erase(core_module_paths, i);
    //            return true;
    //        }
    //    }

    //for each
}
