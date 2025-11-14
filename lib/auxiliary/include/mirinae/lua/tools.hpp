#pragma once

#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}


namespace mirinae {

    class LuaFuncList {

    public:
        LuaFuncList();
        void add(const char* const name, lua_CFunction func);
        const luaL_Reg* data() const;
        void reserve(const size_t s);

    private:
        void append_null_pair();

        std::vector<luaL_Reg> data_;
    };


    class LuaStateView {

    public:
        LuaStateView(lua_State* L) : L_(L) {}

        lua_State* get() const { return L_; }

        void set_global_ptr(const char* name, void* ptr);
        void* find_global_ptr(const char* name);

        void define_metatable(const char* name, const luaL_Reg* functions);
        void define_metatable(const char* name, const LuaFuncList& functions);
        void new_lib(const luaL_Reg* const functions);
        void new_lib(const LuaFuncList& functions);

        void* push_meta_obj(const char* type_name, size_t size);

        template <typename... T>
        int error(const char* fmt, T&&... args) {
            return luaL_error(L_, fmt, args...);
        }

        template <typename T>
        T* check_udata(int idx, const char* type_name) {
            void* ud = luaL_checkudata(L_, idx, type_name);
            return static_cast<T*>(ud);
        }

        template <typename T>
        T* push_meta_obj(const char* const type_name) {
            if (auto ud = this->push_meta_obj(type_name, sizeof(T))) {
                const auto ud_ptr = static_cast<T*>(ud);
                return ud_ptr;
            }

            return nullptr;
        }

        template <typename T>
        int return_meta_obj(const T& src_obj, const char* const type_name) {
            if (auto out = this->push_meta_obj<T>(type_name)) {
                *out = src_obj;
                return 1;
            } else {
                return this->error("Failed to push meta object: %s", type_name);
            }
        }

    private:
        void set_func_to_table(const luaL_Reg* l, int nup);

        lua_State* L_;
    };

}  // namespace mirinae
