#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__ModWarningScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__ModWarningScreen_H__

#include "../Screen.h"
#include "../components/Button.h"
#include "../../Minecraft.h"

// Full-screen warning shown when entering a world whose recorded mod set
// (written on first save) differs from the currently enabled mods.
// Dirt-wall background, warning text on top, "我知道了" button at the bottom
// that returns to the main menu (leaveGame saves the world first), plus an
// "忽略并继续" button that enters the world anyway (confirmWorldEntry takes
// over the level object selectLevel() kept aside).
class ModWarningScreen: public Screen
{
public:
	ModWarningScreen(const std::string& warning)
	:	_warning(warning),
		bOk(0),
		bContinue(0)
	{
	}

	virtual ~ModWarningScreen() {
		delete bOk;
		delete bContinue;
	}

	void init() {
		if (minecraft->useTouchscreen()) {
			bOk = new Touch::TButton(1, "我知道了");
			bContinue = new Touch::TButton(2, "忽略并继续");
		} else {
			bOk = new Button(1, "我知道了");
			bContinue = new Button(2, "忽略并继续");
		}
		buttons.push_back(bOk);
		tabButtons.push_back(bOk);
		buttons.push_back(bContinue);
		tabButtons.push_back(bContinue);
	}

	void setupPositions() {
		bOk->width = 200;
		bOk->x = (width - bOk->width) / 2;
		bOk->y = height - 70;
		bContinue->width = 200;
		bContinue->x = (width - bContinue->width) / 2;
		bContinue->y = height - 38;
	}

	void tick() {}

	void render(int xm, int ym, float a) {
		renderDirtBackground(0);
		drawCenteredString(minecraft->font, "世界模组不匹配!", width/2, height/5 - 12, 0xffff55);
		// Split the warning on the two tag groups so each mod list gets its own line.
		std::string line1, line2;
		const std::string& w = _warning;
		size_t p = w.find('[');
		if (p != std::string::npos) {
			size_t q = w.find(']', p);
			line1 = w.substr(p, (q == std::string::npos ? w.size() : q + 1) - p);
			std::string rest = (q == std::string::npos) ? "" : w.substr(q + 1);
			size_t p2 = rest.find('[');
			if (p2 != std::string::npos) {
				size_t q2 = rest.find(']', p2);
				line2 = rest.substr(p2, (q2 == std::string::npos ? rest.size() : q2 + 1) - p2);
			}
		}
		drawCenteredString(minecraft->font, line1, width/2, height/5 + 16, 0xffffff);
		if (!line2.empty())
			drawCenteredString(minecraft->font, line2, width/2, height/5 + 40, 0xffffff);
		drawCenteredString(minecraft->font, "若继续使用，该世界的模组物品/方块可能异常。", width/2, height/5 + 64, 0xffffff);

		Screen::render(xm, ym, a);
	}

	void buttonClicked(Button* button) {
		if (button->id == bOk->id) {
			minecraft->leaveGame();
		} else if (button->id == bContinue->id) {
			minecraft->confirmWorldEntry();
		}
	};
private:
	std::string _warning;
	Button* bOk;
	Button* bContinue;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__ModWarningScreen_H__*/
