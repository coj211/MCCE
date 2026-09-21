#ifndef NET_MINECRAFT_CLIENT_PLAYER__IInputHolder_H__
#define NET_MINECRAFT_CLIENT_PLAYER__IInputHolder_H__

#include "IMoveInput.h"
#include "ITurnInput.h"
#include "IBuildInput.h"

#include "../../../platform/input/Mouse.h"

class Player;

class IInputHolder: public IConfigListener
{
public:
	IInputHolder()
	:	mousex(0),
		mousey(0),
		alpha(0)
	{}

	virtual ~IInputHolder() {}

	virtual void render(float alpha) {}

	virtual bool allowPicking() { 
		mousex = Mouse::getX();
		mousey = Mouse::getY();
		return Mouse::getButtonState(MouseAction::ACTION_LEFT) == MouseAction::DATA_DOWN;
	}

	void onConfigChanged(const Config& c) {
		getMoveInput()->onConfigChanged(c);
		getTurnInput()->onConfigChanged(c);
		getBuildInput()->onConfigChanged(c);
	}

	virtual IMoveInput*		getMoveInput() = 0;
	virtual ITurnInput*		getTurnInput() = 0;
	virtual IBuildInput*	getBuildInput() = 0;

	// 被某个输入控件「独占」的指针（例如移动版摇杆正在拖的那根手指）。
	// 转向/挖掘判定要跳过它：摇杆推到底时手指会离开底座落进转向区，否则会被
	// 当成「按住转向区」→ 转视角，按住 400ms 还会开始挖方块。
	virtual int getCapturedPointer() { return -1; }

	float mousex, mousey;
	float alpha;
};


class CustomInputHolder: public IInputHolder
{
public:
	CustomInputHolder(IMoveInput* move, ITurnInput* turn, IBuildInput* build)
	{
		setInputs(move, turn, build);
	}

	~CustomInputHolder() {
		delete _move;
		delete _turn;
		delete _build;
	}
	void setInputs(IMoveInput* move, ITurnInput* turn, IBuildInput* build) {
		_move = move;
		_turn = turn;
		_build = build;
	}

	virtual IMoveInput*		getMoveInput()	{ return _move; }
	virtual ITurnInput*		getTurnInput()	{ return _turn; }
	virtual IBuildInput*	getBuildInput() { return _build; }

private:
	IMoveInput*		_move;
	ITurnInput*		_turn;
	IBuildInput*	_build;
};

#endif /*NET_MINECRAFT_CLIENT_PLAYER__IInputHolder_H__*/
