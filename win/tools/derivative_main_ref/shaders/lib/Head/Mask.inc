/*
struct MaterialMask {
	//bool sky;
	bool grass;
	//float leaves;
	
	//bool basic;
};

MaterialMask CalculateMasks(in int materialID) {
	MaterialMask mask;

	//mask.sky		= materialID == 0;
	mask.grass		= materialID == 6 || materialID == 27 || materialID == 28 || materialID == 33 || materialID == 34 || materialID == 35;
	//mask.leaves	= float(materialID == 7);

	//mask.basic		= materialID == 13;

	return mask;
}
*/
struct TranslucentMask {
	bool water;
	bool stainedGlass;
	bool ice;
	bool translucent;
};

TranslucentMask CalculateMasksT(in int materialID) {
	TranslucentMask mask;

	mask.stainedGlass	= materialID == 16;
	mask.water			= materialID == 17;
	mask.ice			= materialID == 18;
	mask.translucent	= mask.stainedGlass || mask.water || mask.ice;

	return mask;
}
