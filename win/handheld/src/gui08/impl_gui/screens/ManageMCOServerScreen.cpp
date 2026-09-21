#include <gui/screens/ManageMCOServerScreen.hpp>
#include <Minecraft.hpp>
#include <cpputils.hpp>
#include <gui/NinePatchLayer.hpp>
#include <gui/PackedScrollContainer.hpp>
#include <gui/buttons/OptionButton.hpp>
#include <gui/elements/Label.hpp>
#include <gui/elements/TextBox.hpp>
#include <gui/screens/CreateWorldScreen.hpp>
#include <gui/screens/PlayScreen.hpp>
#include <rendering/Tesselator.hpp>

ManageMCOServerScreen::ManageMCOServerScreen(const MCOServerListItem& a2) {
	this->item.worldName = "My World";
	this->item.gamemodeName = "creative";
	this->manageServerHeader = 0;
	this->backButton = 0;
	this->resetButton = 0;
	this->field_AC = 0;
	this->openLabel = 0;
	this->serverNameLabel = 0;
	this->invitedPeopleLabel = 0;
	this->field_BC = 0;
	this->field_C0 = 0;
	this->serverNameTextBox = 0;
	this->playerNameTextBox = 0;
	this->field_CC = 0;
	this->field_D0 = 0;
	this->field_F4 = 0;
	this->field_F8 = 0;
	this->item = a2;
}
MCOInviteListItemElement* ManageMCOServerScreen::_addInviteElement(const std::string&) {
	printf("ManageMCOServerScreen::_addInviteElement - not implemented\n"); //TODO
	return 0;
}
void ManageMCOServerScreen::_queryUsernameAndUpdateElement(const std::string&) {
	printf("ManageMCOServerScreen::_queryUsernameAndUpdateElement - not implemented\n"); //TODO
}
void ManageMCOServerScreen::_removeInviteElement(const std::string&) {
	printf("ManageMCOServerScreen::_removeInviteElement - not implemented\n"); //TODO
}
void ManageMCOServerScreen::closeScreen() {
	this->minecraft->setScreen(new PlayScreen(0));
}

ManageMCOServerScreen::~ManageMCOServerScreen() {
	if(this->manageServerHeader) {
		delete this->manageServerHeader;
		this->manageServerHeader = 0;
	}
	safeRemove(this->backButton);
	if(this->field_AC) {
		delete this->field_AC;
		this->field_AC = 0;
	}
	safeRemove(this->openLabel);
	safeRemove(this->serverNameLabel);
	safeRemove(this->serverNameTextBox);
	safeRemove(this->invitedPeopleLabel);
	if(this->field_CC) {
		delete this->field_CC;
		this->field_CC = 0;
	}
	if(this->field_D0) {
		delete this->field_D0;
		this->field_D0 = 0;
	}
	safeRemove(this->playerNameTextBox);
	safeRemove(this->resetButton);
}
void ManageMCOServerScreen::render(int32_t a2, int32_t a3, float a4) {
	this->renderMenuBackground(a4);
	this->field_D4->draw(Tesselator::instance, (float)this->field_D4->x, (float)this->field_D4->y);
	this->field_D0->draw(Tesselator::instance, (float)this->field_CC->x - 1.0, (float)this->field_CC->y - 2.0);
	Screen::render(a2, a3, a4);
}
void ManageMCOServerScreen::init(){
	printf("ManageMCOServerScreen::init - not implemented\n"); //TODO
}
void ManageMCOServerScreen::setupPositions() {
	int32_t width; // r5
	Label* serverNameLabel; // r1
	TextBox* serverNameTextBox; // r0

	width = this->width;
	this->backButton->x = 4;
	width /= 2;
	this->backButton->y = 4;
	this->manageServerHeader->x = 0;
	this->manageServerHeader->y = 0;
	this->manageServerHeader->width = this->width;
	this->manageServerHeader->height = this->backButton->height + 8;
	this->field_D4->setSize((float)this->width - 10.0, (float)((float)this->height - 10.0) - (float)this->manageServerHeader->height);
	this->field_D4->x = 5;
	this->field_D4->y = this->manageServerHeader->y + this->manageServerHeader->height + 5;
	serverNameLabel = this->serverNameLabel;
	serverNameTextBox = this->serverNameTextBox;
	this->openLabel->x = 10;
	serverNameTextBox->x = 10;
	serverNameLabel->x = 10;
	this->serverNameLabel->y = this->field_D4->y + 5;
	this->serverNameTextBox->y = this->serverNameLabel->y + 12;
	this->serverNameTextBox->width = width - 20;
	this->openLabel->y = this->field_D4->y + (int32_t)(float)(this->field_D4->height2 * 0.47);
	this->field_AC->y = this->openLabel->y - 4;
	this->field_AC->x = width - 10 - this->field_AC->width;
	this->field_CC->x = width + 11;
	this->field_CC->y = this->serverNameLabel->y;
	this->field_CC->width = width - 22;
	this->field_CC->height = (int32_t)this->field_D4->height2 - 10;
	this->invitedPeopleLabel->x = this->field_CC->x + (this->field_CC->width - this->invitedPeopleLabel->width) / 2;
	this->invitedPeopleLabel->y = this->field_D4->y + (int32_t)this->field_D4->height2 / 2 - 5;
	this->resetButton->x = this->width - this->backButton->x - this->resetButton->width;
	this->resetButton->y = this->backButton->y;
	this->field_D0->setSize((float)this->field_CC->width + 2.0, (float)this->field_CC->height + 4.0);
	this->field_D8->x = (this->field_D4->x + this->field_CC->x) / 2;
	this->field_D8->y = this->field_D4->y + (int32_t)(float)(this->field_D4->height2 * 0.8);
	this->field_CC->setupPositions();
}
bool_t ManageMCOServerScreen::handleBackEvent(bool_t a2) {
	if(a2) {
		if(!this->supppressedBySubWindow()) {
			this->closeScreen();
			return 1;
		}
		bool_t v4 = 1;
		for(auto&& e: this->elements) {
			if(e->backPressed(this->minecraft, 1)) {
				v4 = 0;
			}
		}
		if(v4) {
			this->closeScreen();
		}
	}
	return 1;
}
void ManageMCOServerScreen::tick(){
	printf("ManageMCOServerScreen::tick - not implemented\n"); //TODO
}
void ManageMCOServerScreen::onTextBoxUpdated(int32_t){
	printf("ManageMCOServerScreen::onTextBoxUpdated - not implemented\n"); //TODO
}
void ManageMCOServerScreen::buttonClicked(Button* a2) {
	if(a2 == this->backButton) {
		this->closeScreen();
	} else if(a2 == this->field_BC) {
		this->playerNameTextBox->setText("");
		this->playerNameTextBox->setFocus(this->minecraft);
	} else if(a2 == this->resetButton) {
		this->minecraft->setScreen(new CreateWorldScreen(WST_MCOGAME_RECREATE, this->item));
	}
}
void ManageMCOServerScreen::mouseClicked(int32_t a2, int32_t a3, int32_t a4) {
	Screen::mouseClicked(a2, a3, a4);
}
void ManageMCOServerScreen::mouseReleased(int32_t a2, int32_t a3, int32_t a4) {
	Screen::mouseReleased(a2, a3, a4);
}
void ManageMCOServerScreen::onFriendItemRemoved(const std::string&){
	printf("ManageMCOServerScreen::onFriendItemRemoved - not implemented\n"); //TODO
}
