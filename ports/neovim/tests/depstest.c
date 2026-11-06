/* links every pure-C dependency of the neovim port and exercises each one */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <msgpack.h>
#include <unibilium.h>
#include <vterm.h>
#include <tree_sitter/api.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <uv.h>

int luaopen_lpeg(lua_State* L);
int luaopen_luv(lua_State* L);
const TSLanguage* tree_sitter_c(void);
const TSLanguage* tree_sitter_lua(void);
const TSLanguage* tree_sitter_vim(void);
const TSLanguage* tree_sitter_vimdoc(void);
const TSLanguage* tree_sitter_query(void);
const TSLanguage* tree_sitter_markdown(void);
const TSLanguage* tree_sitter_markdown_inline(void);

static int total, bad;
#define CHECK(name, cond) do { total++; if (!(cond)) { bad++; printf("FAIL %s\n", name); } else printf("ok   %s\n", name); } while (0)

/* strict: no error nodes allowed. vimdoc reports a MISSING "<" on any non-empty text (checked with a
   native build of the same parser), so it is checked loosely, by root type and a tag node. */
static int ts_parses_ex(const TSLanguage* lang, const char* src, const char* root_type, int strict, const char* must_contain) {
    TSParser* p = ts_parser_new();
    if (!ts_parser_set_language(p, lang)) { ts_parser_delete(p); return 0; }
    TSTree* t = ts_parser_parse_string(p, NULL, src, (uint32_t)strlen(src));
    int ok = t && strcmp(ts_node_type(ts_tree_root_node(t)), root_type) == 0 && (!strict || !ts_node_has_error(ts_tree_root_node(t)));
    if (ok && must_contain) { char* str = ts_node_string(ts_tree_root_node(t)); ok = strstr(str, must_contain) != NULL; free(str); }
    if (t) ts_tree_delete(t);
    ts_parser_delete(p);
    return ok;
}

/* Lua 5.1 has no luaL_requiref: register the opener in package.preload instead */
static void preload(lua_State* L, const char* name, lua_CFunction f) {
    lua_getglobal(L, "package"); lua_getfield(L, -1, "preload");
    lua_pushcfunction(L, f); lua_setfield(L, -2, name);
    lua_pop(L, 2);
}

static int ts_parses(const TSLanguage* lang, const char* src, const char* root_type) { return ts_parses_ex(lang, src, root_type, 1, NULL); }

static int screen_damage(VTermRect r, void* u) { (void)r; (*(int*)u)++; return 1; }

int main(void) {
    /* msgpack: pack then unpack */
    msgpack_sbuffer sbuf; msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk; msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    msgpack_pack_map(&pk, 2);
    msgpack_pack_str(&pk, 3); msgpack_pack_str_body(&pk, "key", 3); msgpack_pack_int(&pk, 42);
    msgpack_pack_str(&pk, 4); msgpack_pack_str_body(&pk, "list", 4);
    msgpack_pack_array(&pk, 3); msgpack_pack_true(&pk); msgpack_pack_nil(&pk); msgpack_pack_double(&pk, 2.5);
    msgpack_unpacked u; msgpack_unpacked_init(&u);
    size_t off = 0;
    int r = msgpack_unpack_next(&u, sbuf.data, sbuf.size, &off);
    CHECK("msgpack roundtrip", r == MSGPACK_UNPACK_SUCCESS && u.data.type == MSGPACK_OBJECT_MAP && u.data.via.map.size == 2 &&
          u.data.via.map.ptr[0].val.via.i64 == 42 && u.data.via.map.ptr[1].val.via.array.size == 3 &&
          u.data.via.map.ptr[1].val.via.array.ptr[2].via.f64 == 2.5);
    msgpack_unpacked_destroy(&u); msgpack_sbuffer_destroy(&sbuf);

    /* unibilium: a terminfo entry built in memory */
    unibi_term* ut = unibi_dummy();
    unibi_set_str(ut, unibi_clear_screen, "\x1b[H\x1b[2J");
    unibi_set_num(ut, unibi_columns, 80);
    char buf[64]; size_t n = unibi_dump(ut, buf, sizeof buf);
    unibi_term* back = n > 0 && n <= sizeof buf ? unibi_from_mem(buf, n) : NULL;
    CHECK("unibilium dump/load", back && unibi_get_num(back, unibi_columns) == 80 && unibi_get_str(back, unibi_clear_screen) &&
          strcmp(unibi_get_str(back, unibi_clear_screen), "\x1b[H\x1b[2J") == 0);
    if (back) unibi_destroy(back);
    unibi_destroy(ut);

    /* libvterm: feed text, read it back from the screen */
    VTerm* vt = vterm_new(10, 40);
    vterm_set_utf8(vt, 1);
    VTermScreen* scr = vterm_obtain_screen(vt);
    int damage = 0;
    static VTermScreenCallbacks cbs; cbs.damage = screen_damage;
    vterm_screen_set_callbacks(scr, &cbs, &damage);
    vterm_screen_reset(scr, 1);
    vterm_input_write(vt, "hello \x1b[1mworld\x1b[m", 18);
    char line[16] = {0};
    VTermRect rect = { 0, 1, 0, 11 };
    vterm_screen_get_text(scr, line, sizeof line, rect);
    CHECK("libvterm screen text", strcmp(line, "hello world") == 0);
    vterm_free(vt);

    /* Lua + lpeg + luv */
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    preload(L, "lpeg", luaopen_lpeg);
    int e = luaL_dostring(L, "local lpeg = require('lpeg')\n"
        "local digits = lpeg.C(lpeg.R('09')^1) * -1\n"
        "return digits:match('12345') == '12345' and digits:match('12a') == nil");
    CHECK("lpeg match", e == 0 && lua_toboolean(L, -1));
    lua_settop(L, 0);
    preload(L, "luv", luaopen_luv);
    e = luaL_dostring(L, "local luv = require('luv')\n"
        "local hits = 0\n"
        "local t = luv.new_timer()\n"
        "t:start(5, 5, function() hits = hits + 1; if hits == 3 then t:stop(); t:close() end end)\n"
        "luv.run()\n"
        "return hits == 3 and luv.hrtime() > 0");
    if (e != 0) printf("lua error: %s\n", lua_tostring(L, -1));
    CHECK("luv timer from Lua", e == 0 && lua_toboolean(L, -1));
    lua_close(L);

    /* tree-sitter with every bundled parser */
    CHECK("ts c", ts_parses(tree_sitter_c(), "int main(void) { return 0; }\n", "translation_unit"));
    CHECK("ts lua", ts_parses(tree_sitter_lua(), "local x = 1\nprint(x)\n", "chunk"));
    CHECK("ts vim", ts_parses(tree_sitter_vim(), "let g:x = 1\n", "script_file"));
    CHECK("ts vimdoc", ts_parses_ex(tree_sitter_vimdoc(), "*tag*\n", "help_file", 0, "(tag"));
    CHECK("ts query", ts_parses(tree_sitter_query(), "(identifier) @name\n", "program"));
    CHECK("ts markdown", ts_parses(tree_sitter_markdown(), "# title\n\ntext\n", "document"));
    CHECK("ts markdown_inline", ts_parses(tree_sitter_markdown_inline(), "some *emph* text", "inline"));

    printf("depstest: %d/%d ok\n", total - bad, total);
    return bad != 0;
}
