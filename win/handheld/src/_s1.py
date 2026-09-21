import io

# SimpleChooseLevelScreen.h
p = "client/gui/screens/SimpleChooseLevelScreen.h"
s = io.open(p, encoding="utf-8").read()
old = """	// Step 2: world type
	Touch::TButton* bOldWorld;
	Touch::TButton* bInfiniteWorld;

	bool hasChosen;
	int chosenGameType; // -1 until chosen

	std::string levelName;
};"""
new = """	// Step 2: world type
	Touch::TButton* bOldWorld;
	Touch::TButton* bInfiniteWorld;

	// Step 3: dimension (mod-defined via Dimension.define)
	Touch::TButton* bDimension;
	Touch::TButton* bCreate;
	int chosenWorldType;   // set at end of step 2
	int chosenDimension;   // 0 = default

	bool hasChosen;
	int chosenGameType; // -1 until chosen

	std::string levelName;

	std::string getDimensionName(int id);
	int nextDimension(int cur);
};"""
assert s.count(old) == 1, "h"
s = s.replace(old, new)
io.open(p, "w", encoding="utf-8", newline="").write(s)
print("h OK")
