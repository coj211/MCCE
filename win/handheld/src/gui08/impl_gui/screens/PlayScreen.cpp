#include <util/IntRectangle.hpp>
#include <gui/screens/PlayScreen.hpp>
#include <Minecraft.hpp>
#include <I18n.hpp>
#include <gui/PackedScrollContainer.hpp>
#include <rendering/Tesselator.hpp>
#include <gui/NinePatchFactory.hpp>
#include <algorithm>
#include <utils.h>
#include <ExternalServerFile.hpp>
#include <ExternalServer.hpp>
#include <network/RakNetInstance.hpp>
#include <RakPeerInterface.h> // getPeer()->Ping 需要完整类型
#include <network/mco/MojangConnector.hpp>
// 0.8.1 GUI 移植：CreateWorldScreen 已接入（创建向导），外部服务器用 0.8.1 AddExternalServerScreen
#include <gui/screens/AddExternalServerScreen.hpp>
#include <gui/screens/CreateWorldScreen.hpp>
#include <gui/elements/LocalServerListItemElement.hpp>
#include <level/storage/LevelStorageSource.hpp>
#include <level/LevelSettings.hpp>
#include <gui/screens/ProgressScreen.hpp>
#include <cpputils.hpp>
#include <gui/elements/Label.hpp>
#include <network/RestService.hpp>
#include <network/mco/MCOParser.hpp>
#include <stdio.h>


// 用户定制：大厅左侧世界截图——记录已加载的截图路径，切换世界时强制重载（idMap 缓存会挡住新截图）
static std::string s_lastShotPath;

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <netdb.h>
  #include <arpa/inet.h>
#endif

static std::string resolveHost(const std::string& host) {
	if (host.empty()) return "";
	struct hostent* he = gethostbyname(host.c_str());
	if (he && he->h_addr_list && he->h_addr_list[0]) {
		return inet_ntoa(*(struct in_addr*)he->h_addr_list[0]);
	}
	return host;
}

PlayScreen::PlayScreen(bool_t a2) {
	PlayScreenState v4 = a2 ? PlayScreenState::ELEVEN : PlayScreenState::ZERO;
	this->field_50 = 0;
	this->field_51 = 0;
	this->header = 0;
	this->backButton = 0;
	this->field_5C = "";
	this->newButton = 0;
	this->externalButton = 0;
	this->frame = 0;
	this->field_74 = 0;
	this->field_78 = 0;
	this->spinner = 0;
	this->editButton = 0;
	this->field_84 = 0;

	this->field_B4 = 0;
	this->field_C4 = "";
	this->field_118 = v4;
	std::string v6 = "Welcome to the Minecraft Realms Alpha! We're still testing out features, but eventually Realms will let up to 10" " Pocket Edition users play together online. It's currently free, and limited to a set amount of servers. \n" "\n" "\n" "Realms will be an optional, paid service once it's released. Have fun!";
	std::string v7 = "Minecraft Realms is currently in a limited alpha test. More servers will be available to register from this page" " as the service is developed, so check back soon.\n" "\n" "Realms servers may be down or be reset while we are working toward the beta release.";
	std::string v8 = "Tap 'New' to create your own Realms server!\n\nFree during alpha.";
	this->setPlayScreenStateSetting(PlayScreenState::ZERO, 0, 0, 0, 0, 0, 0, PlayScreenPanel::NONE, "");
	this->setPlayScreenStateSetting(PlayScreenState::ONE, 0, 0, 0, 1, 0, 0, PlayScreenPanel::MESSAGE, v6);
	this->setPlayScreenStateSetting(PlayScreenState::TWO, 0, 0, 0, 0, 0, 0, PlayScreenPanel::MESSAGE, "");
	this->setPlayScreenStateSetting(PlayScreenState::THREE, 1, 0, 0, 0, 0, 1, PlayScreenPanel::MCO_SERVER_LIST, "");
	this->setPlayScreenStateSetting(PlayScreenState::FOUR, 1, 0, 0, 0, 0, 1, PlayScreenPanel::MESSAGE, v8);
	this->setPlayScreenStateSetting(PlayScreenState::FIVE, 1, 1, 1, 0, 0, 0, PlayScreenPanel::MCO_SERVER_LIST, "");
	this->setPlayScreenStateSetting(PlayScreenState::SIX, 1, 0, 0, 0, 0, 0, PlayScreenPanel::MCO_SERVER_LIST, "");
	this->setPlayScreenStateSetting(PlayScreenState::SEVEN, 1, 0, 0, 0, 0, 0, PlayScreenPanel::MESSAGE, v7);
	this->setPlayScreenStateSetting(PlayScreenState::EIGHT, 0, 0, 0, 0, 0, 0, PlayScreenPanel::MESSAGE, "");
	this->setPlayScreenStateSetting(PlayScreenState::NINE, 0, 0, 0, 0, 0, 0, PlayScreenPanel::JOIN_REALMS_0, "");
	this->setPlayScreenStateSetting(PlayScreenState::TEN, 0, 0, 0, 0, 0, 0, PlayScreenPanel::JOIN_REALMS_1, "");
	this->setPlayScreenStateSetting(PlayScreenState::ELEVEN, 1, 0, 0, 0, 1, 1, PlayScreenPanel::LOCAL_SERVER_LIST, "");
	this->setPlayScreenStateSetting(PlayScreenState::TWELVE, 1, 1, 0, 0, 1, 0, PlayScreenPanel::LOCAL_SERVER_LIST, "");
}
std::shared_ptr<GuiElement> PlayScreen::buildJoinRealmsScreen(bool_t) {
	//TODO
	printf("PlayScreen::buildJoinRealmsScreen - not implemented\n");
	return std::shared_ptr<GuiElement>();
}
std::shared_ptr<GuiElement> PlayScreen::buildLocalServerList() { //TODO returns std::shared_ptr<GuiElement>
	this->field_50 = 0;
	bool isEditMode = this->isEditMode();

	if(!this->field_1F4.get()) {
		this->field_1F4 = std::shared_ptr<PackedScrollContainer>(new PackedScrollContainer(0, 0, 0));
	}
	//TODO check is it actually dynamic_pointer_cast
	std::shared_ptr<PackedScrollContainer> v31 = std::dynamic_pointer_cast<PackedScrollContainer>(this->field_1F4);
	v31.get()->clearAll(); // children.clear() 只清不删：旧元素由容器析构时统一释放，避免双重 delete
	std::unordered_map<int, ExternalServer> servers = *this->minecraft->externalServerFile->getExternalServers();
	for(auto server: servers) {
		LocalServerListItemElement* v32 = new LocalServerListItemElement(this->minecraft, ExternalServer(server.second), isEditMode, this);
		v32->init(this->minecraft);
		v31->addChild(v32);
	}

	for(auto&& p: this->field_88) {
		if(!p.name.IsEmpty()) {
			bool isExternal = false;
			char pAddr[128];
			p.address.ToString(0, pAddr, 0);
			int pPort = p.address.GetPort();
			for(auto& serverPair: servers) {
				const ExternalServer& ext = serverPair.second;
				if ((ext.field_8 == pAddr || resolveHost(ext.field_8) == pAddr) && ext.field_C == pPort) {
					isExternal = true;
					break;
				}
			}
			if (!isExternal) {
				LocalServerListItemElement* v34 = new LocalServerListItemElement(p);
				v34->init(this->minecraft);
				v31->addChild(v34);
			}
		}
	}

	std::vector<LevelSummary> v35;
	this->minecraft->getLevelSource()->getLevelList(v35);
	if(!v35.empty()) {
		std::sort(v35.begin(), v35.end()); //TODO check
	}

	for(auto&& v23: v35) {
		if(v23.id != LevelStorageSource::TempLevelId) {
			LocalServerListItemElement* v32 = new LocalServerListItemElement(this->minecraft, v23, isEditMode);
			v32->init(this->minecraft);
			v31->addChild(v32);
		}
	}
	return this->field_1F4;
}
std::shared_ptr<GuiElement> PlayScreen::buildMCOServerList() {
	if(!this->field_1FC) {
		this->field_1FC = std::shared_ptr<PackedScrollContainer>(new PackedScrollContainer(0, 0, 0));
	}
	std::shared_ptr<PackedScrollContainer> v23 = std::dynamic_pointer_cast<PackedScrollContainer>(this->field_1FC);
	v23->clearAll();
	//TODO
	printf("PlayScreen::buildMCOServerList - not implemented\n");
	return this->field_1FC;
}
std::shared_ptr<GuiElement> PlayScreen::buildMessageScreen() {
	//TODO check
	std::shared_ptr<PackedScrollContainer> v4(new PackedScrollContainer(0, 0, 0));
	PlayScreenStateSetting* stateData = this->getStateData(this->field_114);
	v4->addChild(std::shared_ptr<Label>(new Label(stateData->field_C, this->minecraft, -1, 5, 2, this->field_214->width, 1)).get());
	v4->setupPositions();
	return v4;
}
void PlayScreen::closeScreen() {
	this->minecraft->cancelLocateMultiplayer();
	this->minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
}
PlayScreenState PlayScreen::getState() {
	return this->field_114;
}
PlayScreenStateSetting* PlayScreen::getStateData(PlayScreenState a2) {
	return &this->field_11C[a2];
}
bool_t PlayScreen::isEditMode() {
	// 用户定制：Edit 常驻开启（列表项一直显示删除/设置按钮）
	return true;
}
bool_t PlayScreen::isLocalPlayScreen() {
	return 0;
}
void PlayScreen::joinMCOServer(MCOServerListItem) {
	//TODO join mco server
	printf("PlayScreen::joinMCOServer - not implemented\n");
}
void PlayScreen::resetBaseButtons() {
	this->buttons.clear();
	this->buttons.emplace_back(this->header);
	this->buttons.emplace_back(this->backButton);
	this->buttons.emplace_back(this->newButton);
	this->buttons.emplace_back(this->editButton);
	this->buttons.emplace_back(this->externalButton);
}
void PlayScreen::resetCurrentlyWaitingMCOCancelButton(void) {
	if(this->field_B4) {
		Button** but = std::find(this->buttons.data(), this->buttons.data() + this->buttons.size(), this->field_B4);
		if(but != (this->buttons.data() + this->buttons.size())) {
			this->buttons.erase(this->buttons.begin() + (but - this->buttons.data())); //TODO check
		}
	}
	this->field_B4 = 0;
}
void PlayScreen::setMainPanel(PlayScreenPanel a2) {
	this->elements.clear();
	switch(a2) {
		case PlayScreenPanel::MESSAGE:
			this->field_1EC = this->buildMessageScreen();
			this->field_214 = this->field_1EC;
			this->elements.emplace_back(this->field_1EC.get());
			break;
		case PlayScreenPanel::LOCAL_SERVER_LIST:
			this->field_1F4 = this->buildLocalServerList();
			this->field_214 = this->field_1F4;
			this->elements.emplace_back(this->field_1F4.get());
			break;
		case PlayScreenPanel::MCO_SERVER_LIST:
			this->field_1FC = this->buildMCOServerList();
			this->field_214 = this->field_1FC;
			this->elements.emplace_back(this->field_1FC.get());
			break;
		case PlayScreenPanel::JOIN_REALMS_0:
			this->field_204 = this->buildJoinRealmsScreen(0);
			this->field_214 = this->field_204;
			this->elements.emplace_back(this->field_204.get());
			break;
		case PlayScreenPanel::JOIN_REALMS_1:
			this->field_20C = this->buildJoinRealmsScreen(1);
			this->field_214 = this->field_20C;
			this->elements.emplace_back(this->field_20C.get());
			break;
		default:
			break;
	}
	this->setupPositions();
}
void PlayScreen::setPlayScreenSate(PlayScreenState a2, bool_t a3) {
	if(a3 || a2 != this->field_114) {
		this->resetBaseButtons();
		this->updateHeaderItems(a2);
		this->field_114 = a2;
		this->field_118 = a2;
		PlayScreenStateSetting* state = this->getStateData(a2);
		this->setMainPanel(state->panel);
	}
}
void PlayScreen::setPlayScreenStateSetting(PlayScreenState state, bool_t a3, bool_t a4, bool_t a5, bool_t a6, bool_t a7, bool_t a8, PlayScreenPanel a9, const std::string& a10) {
	PlayScreenStateSetting v16(a3, a4, a5, a6, a7, a8, a9);
	v16.field_C = a10;

	this->field_11C[state] = v16;
}
void PlayScreen::signOut() {
	//TODO
	printf("PlayScreen::signOut - not implemented\n");
}
void PlayScreen::updateHeaderItems(PlayScreenState a2) {
	this->newButton->setActiveAndVisibility(this->getStateData(a2)->showNewButton);
	this->editButton->setActiveAndVisibility(this->getStateData(a2)->showEditButton);
	if(this->getStateData(a2)->field_1) this->field_84 = this->editButton;
	else this->field_84 = 0;
	this->externalButton->setActiveAndVisibility(this->getStateData(a2)->showExternalButton);
}

void PlayScreen::updateMCOServerList() {
	//TODO
	printf("PlayScreen::updateMCOServerList - not implemented\n");
}
void PlayScreen::updateMCOStatus() {
	if(this->minecraft->mojangConnector->getConnectionStatus() && !this->field_A4 && this->minecraft->mojangConnector->getConnectionStatus()) {
		safeStopAndRemove<std::shared_ptr<RestRequestJob>>(this->field_9C);
		this->field_9C = RestRequestJob::CreateJob(RRT_GET, this->minecraft->mojangConnector->getMCOSercice(), this->minecraft);
		this->field_9C->setMethod("/info/status");
		RestRequestJob::launchRequest(
			this->field_9C,
			this->minecraft->mojangConnector->getThreadCollection(),
			[this](int32_t a2, const std::string& a3, const RestCallTagData& a4, std::shared_ptr<RestRequestJob> v11) { //automatically copies it?
				bool buyServerEnabled = 0, createServersEnabled = 0, serviceEnabled = 0;
				this->minecraft->mojangConnector->getMCOParser()->parseStatus(a3, buyServerEnabled, createServersEnabled, serviceEnabled);
				this->minecraft->mojangConnector->setMCOServiceEnabled(serviceEnabled);
				this->minecraft->mojangConnector->setMCOCreateServersEnabled(createServersEnabled);
				safeStopAndRemove<std::shared_ptr<RestRequestJob>>(this->field_9C);
				if(serviceEnabled) {
					safeStopAndRemove<std::shared_ptr<RestRequestJob>>(this->field_9C);
					this->updateRealmsState();
				} else {
					this->minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
				}
			},
			[this](bool, bool, int32_t, const std::string&, const RestCallTagData&, std::shared_ptr<RestRequestJob> v9){ //same as in prev func
				safeStopAndRemove<std::shared_ptr<RestRequestJob>>(this->field_9C);
			}
		);
	}
}
void PlayScreen::updateRealmsState() {
	bool createServersEnabled = this->minecraft->mojangConnector->isMCOCreateServersEnabled();
	std::shared_ptr<std::unordered_map<long long, MCOServerListItem>> v6 = this->minecraft->mojangConnector->getMCOServerList();
	if(!v6) {
		if(createServersEnabled) {
			this->setPlayScreenSate(FOUR, 1);
		} else {
			this->setPlayScreenSate(SEVEN, 1);
		}
	} else {
		if(!createServersEnabled) {
			if(v6->size()) {
				this->setPlayScreenSate(SIX, 1);
			}else{
				this->setPlayScreenSate(SEVEN, 1);
			}
		}else{
			if(v6->size()) {
				this->setPlayScreenSate(THREE, 1);
			}else{
				this->setPlayScreenSate(FOUR, 1);
			}
		}
	}
}

PlayScreen::~PlayScreen() {
	if(this->header) {
		delete this->header;
		this->header = 0;
	}
	safeRemove<Touch::TButton>(this->backButton);
	safeRemove<Touch::TButton>(this->newButton);
	safeRemove<Touch::TButton>(this->externalButton);
	if(this->editButton) {
		delete this->editButton;
		this->editButton = 0;
	}
	safeRemove<NinePatchLayer>(this->field_74);
	safeRemove<NinePatchLayer>(this->field_78);
	safeRemove<NinePatchLayer>(this->frame);
	if(this->spinner) {
		delete this->spinner;
		this->spinner = 0;
	}
	safeStopAndRemove<std::shared_ptr<RestRequestJob>>(this->field_AC);
	safeStopAndRemove<std::shared_ptr<RestRequestJob>>(this->field_9C);
	safeStopAndRemove<std::shared_ptr<RestRequestJob>>(this->field_94);
	safeStopAndRemove<std::shared_ptr<RestRequestJob>>(this->field_A4);
	// 显式释放剩余成员(逆声明序),定位堆损坏(0xc0000374)
	this->field_214.reset();
	this->field_20C.reset();
	this->field_204.reset();
	this->field_1FC.reset();
	this->field_1F4.reset();
	this->field_1EC.reset();
	this->field_AC.reset();
	this->field_A4.reset();
	this->field_9C.reset();
	this->field_94.reset();
	this->field_88.clear();
	this->field_54.reset();
	this->field_CC.worldName.clear();
	this->field_CC.gamemodeName.clear();
	this->field_CC.field_10.clear();
	this->field_CC.field_20.clear();
	this->field_CC.field_2C.clear();
	this->field_C4.clear();
}

void PlayScreen::render(int32_t mx, int32_t my, float pt) {
	this->renderMenuBackground(pt);
	this->frame->draw(Tesselator::instance, this->field_214.get()->x - 3, this->field_214.get()->y - 3);

	// 用户定制：选中世界 → 左侧显示该世界保存时的截图（无截图不显示）
	LocalServerListItemElement* selItem = getSelectedLevelItem();
	if (selItem && selItem->levelSummary && !selItem->server && !selItem->field_3C) {
		std::string shotPath = this->minecraft->externalStoragePath +
			"/games/com.mojang/minecraftWorlds/" + selItem->levelSummary->id +
			"/screenshot.png";
		// 切到不同世界/截图更新时强制重载（idMap 缓存会挡住新截图）
		if (shotPath != s_lastShotPath) {
			if (!s_lastShotPath.empty())
				this->minecraft->textures->unloadTexture(s_lastShotPath);
			s_lastShotPath = shotPath;
		}
		TextureId texId = this->minecraft->textures->loadTexture(shotPath, false);
		if (Textures::isTextureIdValid(texId)) {
			const TextureData* td = this->minecraft->textures->getTemporaryTextureData(texId);
			if (td && td->w > 0 && td->h > 0) {
				// 左半区域：与右侧世界列表一样大（x 从 10 到列表左边缘-10，高度与列表一致）
				int32_t listX = this->field_214.get()->x;
				int32_t listY = this->field_214.get()->y;
				int32_t listW = this->field_214.get()->width;
				int32_t listH = this->field_214.get()->height;
				int32_t regionX = 4;    // 左移一点点（原来 10）
				int32_t regionY = listY;
				int32_t regionW = listW;   // 与列表同宽
				int32_t regionH = listH;   // 与列表同高
				// cover：放大填满区域，居中，UV 裁切超出部分
				float scale = (float)regionW / td->w;
				float scaleH = (float)regionH / td->h;
				if (scaleH > scale) scale = scaleH;
				float drawW = td->w * scale;
				float drawH = td->h * scale;
				float drawX = regionX + (regionW - drawW) / 2;
				float drawY = regionY + (regionH - drawH) / 2;
				// UV 裁切到区域
				float u0 = (regionX - drawX) / drawW;
				float u1 = u0 + regionW / drawW;
				float v0 = (regionY - drawY) / drawH;
				float v1 = v0 + regionH / drawH;
				this->minecraft->textures->bind(texId);
				Tesselator& t = Tesselator::instance;
				t.begin();
				t.color(0xffffffff);
				float fx0 = (float)regionX, fy0 = (float)regionY;
				float fx1 = (float)(regionX + regionW), fy1 = (float)(regionY + regionH);
				t.vertexUV(fx0, fy0, this->blitOffset, u0, v0);
				t.vertexUV(fx0, fy1, this->blitOffset, u0, v1);
				t.vertexUV(fx1, fy1, this->blitOffset, u1, v1);
				t.vertexUV(fx1, fy0, this->blitOffset, u1, v0);
				t.draw();
			}
		}
	}

	Screen::render(mx, my, pt);
	this->spinner->render(this->minecraft, mx, my);
}
void PlayScreen::init() {
	// 每次进大厅强制重载上次会话的截图：清掉 idMap/loadedImages 缓存，
	// 否则同路径 loadTexture 命中旧缓存 → 永远显示旧截图（必须退游戏才刷新）
	if (!s_lastShotPath.empty())
		this->minecraft->textures->unloadTexture(s_lastShotPath);
	s_lastShotPath.clear();
	this->header = new Touch::THeader(0, this->field_118 == ELEVEN ? I18n::get("play.title.game") : I18n::get("play.title.realms"));
	this->backButton = new Touch::TButton(1, I18n::get("gui.back"), 0);
	this->newButton = new Touch::TButton(2, I18n::get("play.new"), 0);
	this->newButton->width = 100;
	this->externalButton = new Touch::TButton(2626, I18n::get("play.external"), this->minecraft);
	this->backButton->width = 38;
	this->backButton->height = 18;
	this->externalButton->width = 50;
	this->externalButton->height = this->backButton->height;
	this->backButton->init(this->minecraft);
	IntRectangle a = {8, 32, 8, 8};
	IntRectangle b = {0, 32, 8, 8};
	this->newButton->init(this->minecraft, "gui/spritesheet.png", a, b, 2, 2, this->newButton->width, this->newButton->height);
	this->newButton->height = this->backButton->height;
	this->newButton->width = 38;
	NinePatchFactory a1(this->minecraft->textures, "gui/spritesheet.png");
	NinePatchLayer* lay = a1.createSymmetrical({34, 43, 14, 14}, 3, 3, 32, 32);
	// 09b · UI 覆盖：世界列表底板 -> mod UI.skin("play.frame", ...) 可换皮肤
	lay->setUiSkinKey("play.frame");
	this->frame = lay;
	NinePatchLayer* lay2 = a1.createSymmetrical({8, 32, 8, 8}, 2, 2, this->backButton->width, this->backButton->height);
	this->field_74 = lay2;
	this->field_78 = a1.createSymmetrical({0, 32, 8, 8}, 2, 2, this->backButton->width, this->backButton->height);
	this->editButton = new CategoryButton(I18n::get("play.edit"), 3, this->field_74, this->field_78, &this->field_84);
	this->editButton->width = this->backButton->width;
	this->editButton->height = this->backButton->height;
	this->editButton->setYOffset(this->editButton->height / 2 - 4);
	this->spinner = new Spinner();
	this->spinner->setActiveAndVisibility(0);
	this->minecraft->locateMultiplayer();
	this->field_BC = getTimeS();
	this->field_1F4 = this->buildLocalServerList();
	this->field_214 = this->field_1F4;
	this->field_88 = this->minecraft->raknetInstance->getServerList();
	if(this->field_118) {
		this->setPlayScreenSate(this->field_118, 1);
	} else {
		MojangConnectionStatus cs = this->minecraft->mojangConnector->getConnectionStatus();
		if(cs == STATUS_0) {
			this->setPlayScreenSate(ONE, 1);
		} else if(cs == STATUS_1) {
			this->setPlayScreenSate(TWO, 1);
		} else {
			this->updateRealmsState();
		}
	}

	if((uint32_t)(this->field_114 - 3) <= 1 || this->field_114 == SIX || this->field_114 == SEVEN) { //67 leaks
		this->updateMCOStatus();
		this->updateMCOServerList();
	}
}

void PlayScreen::setupPositions() {
	this->backButton->x = 4;
	this->backButton->y = 4;

	// 用户定制：Edit 隐身（不显示，Edit 模式常驻由 isEditMode()=true 实现）
	this->editButton->x = -100;
	this->editButton->y = 4;
	this->editButton->visible = 0;

	this->header->x = 0;
	this->header->y = 0;
	this->header->width = this->width;
	this->header->height = this->backButton->height + 8;

	// 用户定制：新建 + 外部（服务器）居右，占原来「新建 + Edit」的位置
	this->newButton->x = this->width - this->editButton->width - 8 - this->newButton->width;
	this->newButton->y = 4;

	this->externalButton->x = this->width - this->editButton->width - 4;
	this->externalButton->y = 4;

	this->spinner->x = this->width - 4 - this->spinner->width - this->editButton->width;
	this->spinner->y = 9;

	// 用户定制：世界列表整体缩到右半边（宽度减半靠右，左边留空），顶部栏不动
	int32_t listW = (this->width - 20) / 2;
	this->field_214.get()->x = this->width - listW - 10;
	this->field_214.get()->y = this->header->height + 6;
	this->field_214.get()->width = listW;
	this->field_214.get()->height = this->height - (this->header->height + 6) - 6;
	this->field_214.get()->setupPositions();
	this->frame->setSize(this->field_214.get()->width + 6, this->field_214.get()->height + 6);
}
bool_t PlayScreen::handleBackEvent(bool_t a2) {
	if(!a2) this->closeScreen();
	return 1;
}
void PlayScreen::tick() {
	if(this->field_50) {
		this->buildLocalServerList();
	}
	if(this->field_51) {
		safeStopAndRemove<std::shared_ptr<RestRequestJob>>(this->field_AC);
		this->updateRealmsState();
		this->field_51 = 0;
	}

	if(this->field_118 != this->field_114) {
		this->setPlayScreenSate(this->field_118, 1);
	}
	PlayScreenStateSetting* stateData = this->getStateData(this->field_114);
	double v5 = this->field_BC;
	if(stateData->panel == MCO_SERVER_LIST) {
		if(v5 + 10 < getTimeS()) {
			this->field_BC = getTimeS();
			this->updateMCOServerList();
		}
	} else if(v5 + 1 < getTimeS()) {
		this->field_BC = getTimeS();
		if (this->minecraft->raknetInstance && this->minecraft->raknetInstance->getPeer() && this->minecraft->externalServerFile) {
			auto* extMap = this->minecraft->externalServerFile->getExternalServers();
			if (extMap) {
				for (auto& pair : *extMap) {
					const ExternalServer& ext = pair.second;
					if (!ext.field_8.empty()) {
						this->minecraft->raknetInstance->getPeer()->Ping(ext.field_8.c_str(), ext.field_C, 0, 0);
					}
				}
			}
		}
		const ServerList& v8 = this->minecraft->raknetInstance->getServerList();
		if(this->field_88.size() == v8.size()) {
			int v11 = this->field_88.size() - 1;
			while(v11 >= 0) {
				if(this->field_88[v11].address != v8.at(v11).address || this->field_88[v11].name != v8.at(v11).name) {
					this->field_88 = v8;
					this->buildLocalServerList();
				}
				--v11;
			}
		} else {
			this->field_88 = v8;
			this->buildLocalServerList();
		}
	}

	Screen::tick();
}
void PlayScreen::onMojangConnectorStatus(MojangConnectionStatus a2) {
	if((uint32_t)(this->field_114 - 11) > 1) {
		if(a2 == MojangConnectionStatus::STATUS_1) {
			this->setPlayScreenSate(PlayScreenState::TWO, 1);
			return;
		}
		if(a2 != 2) {
			if(a2) return;
			this->setPlayScreenSate(PlayScreenState::ONE, 1);
			return;
		}
		this->updateRealmsState();
		this->updateMCOStatus();
		this->updateMCOServerList();
	}
}
void PlayScreen::buttonClicked(Button* a2) {
	if(a2 == this->backButton) {
		this->buildLocalServerList();
		this->closeScreen();
	} else if(a2 == this->header) {
		this->buildLocalServerList();
	} else if(a2 == this->newButton) {
		if(this->field_114 == PlayScreenState::ELEVEN) {
			// 0.8.1 GUI 移植：打开创建向导（世界名/种子/游戏模式/世界类型）
			this->minecraft->setScreen(new CreateWorldScreen(WST_LOCALGAME, MCOServerListItem()));
		} else {
			// MCO(Realms) 未移植
			this->setPlayScreenSate(this->field_114, 1);
		}
	} else if(a2 == this->editButton) {
		if(a2 == this->field_84) a2 = 0;
		this->field_84 = a2;
		switch(this->field_114) {
			case PlayScreenState::THREE:
			case PlayScreenState::FOUR:
			case PlayScreenState::SIX:
			case PlayScreenState::SEVEN:
				this->setPlayScreenSate(PlayScreenState::FIVE, 1);
				break;
			case PlayScreenState::FIVE:
				this->updateRealmsState();
				return;
			case PlayScreenState::ELEVEN:
				this->setPlayScreenSate(PlayScreenState::TWELVE, 1);
				break;
			case PlayScreenState::TWELVE:
				this->setPlayScreenSate(PlayScreenState::ELEVEN, 1);
				break;
			default:
				return;
		}
	} else if(a2 == this->field_B4) {
		this->field_51 = 1;
	}else if(a2 == this->externalButton){
		this->minecraft->setScreen(new AddExternalServerScreen());
	}
}
void PlayScreen::mouseClicked(int32_t a2, int32_t a3, int32_t a4) {
	Screen::mouseClicked(a2, a3, a4);
}
void PlayScreen::mouseReleased(int32_t a2, int32_t a3, int32_t a4) {
	Screen::mouseReleased(a2, a3, a4);
}
