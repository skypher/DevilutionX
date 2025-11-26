#include "lua/modules/towners.hpp"

#include <optional>
#include <string>
#include <utility>

#include <sol/sol.hpp>

#include "engine/point.hpp"
#include "levels/gendung.h"
#include "lua/metadoc.hpp"
#include "player.h"
#include "towners.h"

namespace devilution {
namespace {

const char *const TownerTableNames[NUM_TOWNER_TYPES] {
	"griswold",
	"pepin",
	"deadguy",
	"ogden",
	"cain",
	"farnham",
	"adria",
	"gillian",
	"wirt",
	"cow",
	"lester",
	"celia",
	"nut",
};

std::string SetTownerPosition(_talker_id townerId, int x, int y)
{
	Towner *towner = GetTowner(townerId);
	if (towner == nullptr) return "Towner not found";

	// Clear old position in dMonster
	dMonster[towner->position.x][towner->position.y] = 0;

	// Set new position
	towner->position = Point { x, y };

	// Mark new position in dMonster (towner index + 1)
	for (size_t i = 0; i < NUM_TOWNERS; ++i) {
		if (&Towners[i] == towner) {
			dMonster[x][y] = static_cast<int16_t>(i + 1);
			break;
		}
	}

	return "Position set";
}

void PopulateTownerTable(_talker_id townerId, sol::table &out)
{
	LuaSetDocFn(out, "position", "()",
	    "Returns towner coordinates",
	    [townerId]() -> std::optional<std::pair<int, int>> {
		    const Towner *towner = GetTowner(townerId);
		    if (towner == nullptr) return std::nullopt;
		    return std::make_pair(towner->position.x, towner->position.y);
	    });

	LuaSetDocFn(out, "setPosition", "(x: number, y: number)",
	    "Sets towner coordinates",
	    [townerId](int x, int y) -> std::string {
		    return SetTownerPosition(townerId, x, y);
	    });
}
} // namespace

sol::table LuaTownersModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	for (uint8_t townerId = TOWN_SMITH; townerId < NUM_TOWNER_TYPES; ++townerId) {
		sol::table townerTable = lua.create_table();
		PopulateTownerTable(static_cast<_talker_id>(townerId), townerTable);
		LuaSetDoc(table, TownerTableNames[townerId], /*signature=*/"", TownerLongNames[townerId], std::move(townerTable));
	}
	return table;
}

} // namespace devilution
