#pragma once


struct lua_State;


namespace mirinae {

    using luaCFunc_t = int (*)(lua_State*);


    class ScriptEngine;

}  // namespace mirinae
