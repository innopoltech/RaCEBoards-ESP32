#include "rpc_function.h"

static rpc_dict_t rpc_dict = {0,};

bool rpc_function_init(void)
{   
    rpc_function_deinit();

    //Set all empty
    for(uint16_t i=0; i<RPC_DICT_LEN; i++)
        rpc_dict.empty_element[i] = true;

    return true;
}

bool rpc_function_deinit(void)
{
    //Clear all
    memset((void*)&rpc_dict, 0, sizeof(rpc_dict));

    return true;
}

int16_t rpc_function_find(char* name)
{   
    //Find index empty slot
    if(name == NULL)
    {   
        for(uint16_t i=0; i<RPC_DICT_LEN; i++)
        {
            if( rpc_dict.empty_element[i] == true)
                return i;
        }
    }
    //Find a function index by name 
    else
    {
        for(uint16_t i=0; i<RPC_DICT_LEN; i++)
        {
            if( strcmp(rpc_dict.dict_element[i].name, name) == 0)
                return i;
        }
    }

    return -1;
}

function_t rpc_function_ptr_from_indx(int16_t indx)
{
    //Check index
    if(indx > RPC_DICT_LEN-1)
        return NULL;
    if(indx < 0)
        return NULL;

    return rpc_dict.dict_element[indx].function;
}

bool rpc_function_add(char* name, void* function_ptr)
{   
    //Find empty slot
    int16_t index = rpc_function_find(NULL);
    if(index<0)
    {
        printf("RPC_FUNCTION - No empty slots!\n");
        return false;
    }

    //Fill element
    rpc_dict.dict_element[index].function = (function_t)function_ptr;
    strncpy(rpc_dict.dict_element[index].name, name, RPC_DICT_ELEMENT_NAME_LEN);

    rpc_dict.empty_element[index] = false;

    printf("RPC_FUNCTION - Register function '%s'\n", rpc_dict.dict_element[index].name);
    return true;
}

bool rpc_function_delete(char* name)
{
    //Find function index
    int16_t index = rpc_function_find(name);
    if(index<0)
    {
        printf("RPC_FUNCTION - Function '%s' not registered!\n", name);
        return false;
    }

    //Clear
    rpc_dict.dict_element[index].function = NULL;
    memset(rpc_dict.dict_element[index].name, 0, RPC_DICT_ELEMENT_NAME_LEN);

    rpc_dict.empty_element[index] = true;

    printf("RPC_FUNCTION - Delete function '%s'\n", name);
    return true;
}