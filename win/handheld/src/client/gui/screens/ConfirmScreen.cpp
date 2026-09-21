#include "ConfirmScreen.h"
#include "../components/Button.h"
#include "../../Minecraft.h"

ConfirmScreen::ConfirmScreen(Screen* parent_, const std::string& title1_, const std::string& title2_, int id_)
:   parent(parent_),
	title1(title1_),
	title2(title2_),
	id(id_),
	yesButtonText("Ok"),
	noButtonText("Cancel"),
	yesButton(0),
	noButton(0)
{
}

ConfirmScreen::ConfirmScreen(Screen* parent_, const std::string& title1_, const std::string& title2_, const std::string& yesButton_, const std::string& noButton_, int id_ )
:   parent(parent_),
	title1(title1_),
	title2(title2_),
	id(id_),
	yesButtonText(yesButton_),
	noButtonText(noButton_)
{
}

ConfirmScreen::~ConfirmScreen() {
	delete yesButton;
	delete noButton;
}

void ConfirmScreen::init()
{
	// 0.8.1 GUI 移植：统一使用 Touch::TButton（0.8.1 触屏风格按钮），不再依赖 useTouchscreen
	yesButton = new Touch::TButton(0, 0, 0, yesButtonText, minecraft);
	noButton  = new Touch::TButton(1, 0, 0, noButtonText, minecraft);
	((Touch::TButton*)yesButton)->init(minecraft);
	((Touch::TButton*)noButton)->init(minecraft);

	buttons.push_back(yesButton);
	buttons.push_back(noButton);

	tabButtons.push_back(yesButton);
	tabButtons.push_back(noButton);
}

void ConfirmScreen::setupPositions() {
	const int ButtonWidth = 120;
	const int ButtonHeight = 24;
	yesButton->x = width / 2 - ButtonWidth - 4;
	yesButton->y = height / 6 + 72;
	noButton->x = width / 2 + 4;
	noButton->y = height / 6 + 72;
	yesButton->width = noButton->width = ButtonWidth;
	yesButton->height = noButton->height = ButtonHeight;
}

bool ConfirmScreen::handleBackEvent(bool isDown) {
	if (!isDown)
		postResult(false);
	return true;
}

void ConfirmScreen::render( int xm, int ym, float a )
{
	renderDirtBackground(0); // 0.8.1 风格：泥土背景（不透明，遮住后面画面）

	drawCenteredString(font, title1, width / 2, 50, 0xffffff);
	drawCenteredString(font, title2, width / 2, 70, 0xffffff);

	super::render(xm, ym, a);
}

void ConfirmScreen::buttonClicked( Button* button )
{
	postResult(button->id == 0);
}

void ConfirmScreen::postResult(bool isOk)
{
	parent->confirmResult(isOk, id);
}
