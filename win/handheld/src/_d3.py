import io

# 1. Dimension.cpp getNew hook
p = "world/level/dimension/Dimension.cpp"
s = io.open(p, encoding="utf-8").read()

old_inc = '#include "../tile/Tile.h"\n#include "../../../util/Mth.h"'
new_inc = '#include "../tile/Tile.h"\n#include "../../../util/Mth.h"\n#ifdef _WIN32\n#include "../../../mod/ModEngine.h"\n#endif'
assert s.count(old_inc) == 1, "dim inc"
s = s.replace(old_inc, new_inc)

old_getnew = """Dimension* Dimension::getNew( int id )
{
	if (id == NORMAL) return new Dimension();
	if (id == NORMAL_DAYCYCLE) return new NormalDayCycleDimension();
	return NULL;
}"""
new_getnew = """Dimension* Dimension::getNew( int id )
{
	if (id == NORMAL) return new Dimension();
	if (id == NORMAL_DAYCYCLE) return new NormalDayCycleDimension();
#ifdef _WIN32
	// Mod-defined (scripted) dimensions.
	if (ModEngine::instance) {
		Dimension* d = ModEngine::instance->createScriptedDimension(id);
		if (d) return d;
	}
#endif
	return NULL;
}"""
assert s.count(old_getnew) == 1, "getNew"
s = s.replace(old_getnew, new_getnew)
io.open(p, "w", encoding="utf-8", newline="").write(s)
print("dim OK")

# 2. LevelSettings.h 加 dimensionId
p2 = "world/level/LevelSettings.h"
s2 = io.open(p2, encoding="utf-8").read()
old = """    LevelSettings(long seed, int gameType, int worldType = WorldType::Old)
    :   seed(seed),
        gameType(gameType),
        worldType(worldType)
    {
    }"""
new = """    LevelSettings(long seed, int gameType, int worldType = WorldType::Old, int dimensionId_ = 0)
    :   seed(seed),
        gameType(gameType),
        worldType(worldType),
        dimensionId(dimensionId_)
    {
    }"""
assert s2.count(old) == 1, "settings ctor"
s2 = s2.replace(old, new)
old2 = """    int getWorldType() const {
        return worldType;
    }"""
new2 = """    int getWorldType() const {
        return worldType;
    }

    int getDimensionId() const {
        return dimensionId;
    }"""
assert s2.count(old2) == 1, "settings getter"
s2 = s2.replace(old2, new2)
# 加成员（找 seed 成员）
old3 = """    long seed;
    int gameType;
    int worldType;"""
new3 = """    long seed;
    int gameType;
    int worldType;
    int dimensionId;"""
assert s2.count(old3) == 1, "settings member"
s2 = s2.replace(old3, new3)
io.open(p2, "w", encoding="utf-8", newline="").write(s2)
print("settings OK")
