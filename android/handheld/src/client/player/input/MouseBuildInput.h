#ifndef NET_MINECRAFT_CLIENT_PLAYER_INPUT_MouseBuildInput_H__
#define NET_MINECRAFT_CLIENT_PLAYER_INPUT_MouseBuildInput_H__

#include "IBuildInput.h"
#include "../../../platform/input/Mouse.h"

/** A Mouse Build input */
class MouseBuildInput : public IBuildInput {
public:
	MouseBuildInput()
	:	_prevLeftDown(false),
		_prevRightDown(false)
	{}

	virtual bool tickBuild(Player* p, BuildActionIntention* bai) {
		bool leftDown = Mouse::getButtonState(MouseAction::ACTION_LEFT) != 0;
		bool firstPress = leftDown && !_prevLeftDown;
		_prevLeftDown = leftDown;

		if (leftDown) {
			// On first click: trigger attack/first-remove so handleBuildAction is called
			// On hold: continuous remove for block breaking (handleMouseDown handles it)
			if (firstPress)
				*bai = BuildActionIntention(BuildActionIntention::BAI_FIRSTREMOVE | BuildActionIntention::BAI_ATTACK);
			else
				*bai = BuildActionIntention(BuildActionIntention::BAI_REMOVE);
			return true;
		}
		bool rightDown = Mouse::getButtonState(MouseAction::ACTION_RIGHT) != 0;
		bool rightFirstPress = rightDown && !_prevRightDown;
		_prevRightDown = rightDown;
		if (rightDown) {
			if (rightFirstPress) {
				// First frame: place a block / open a door / start eating or
				// drawing. Exactly one placement.
				*bai = BuildActionIntention(BuildActionIntention::BAI_BUILD | BuildActionIntention::BAI_INTERACT);
			} else if (p && p->isUsingItem()) {
				// Held while already using (bow drawing / eating): keep the
				// item in use so it keeps drawing/eating.
				*bai = BuildActionIntention(BuildActionIntention::BAI_INTERACT);
			} else {
				// Held but NOT mid-use (e.g. a block in hand): don't keep
				// firing build actions, otherwise one click places a wall.
				return false;
			}
			return true;
		}
		return false;
	}
private:
	bool _prevLeftDown;
	bool _prevRightDown;
};

#endif /*NET_MINECRAFT_CLIENT_PLAYER_INPUT_MouseBuildInput_H__*/
