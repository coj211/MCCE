
// ============================================================
//  v3.2 多人化补丁(引擎 multiplayer-dimensions API)
//  1) 维度切换改为服务器/引擎权威:level.changePlayerDimension
//     (单机=本地迁移+切渲染世界;多人 host=SSNH 按玩家路由,
//      加入者=服务器发 SetDimensionPacket 通知重载维度)。
//  2) 只有主机进程允许发起维度切换;加入者(joiner)的维度完全
//     由服务器安排,绝不自己 resetChunks/travelTo(否则两端打架)。
//  3) 落点:切换后由引擎按需生成区块,mod 找地修正 y 并落地。
// ============================================================
var _trav = null;   // 本地玩家维度切换中 {dim,x,z,tries}

function travelTo(dim, x, z, radius) {
	if (typeof level.isHost !== "function" || !level.isHost()) {
		modLog("[天境] 非主机:忽略本地自动传送(维度由服务器管理)");
		return;
	}
	if (_travelGuard) return;
	_travelGuard = true;
	if (level.stopMusic) level.stopMusic();
	ui.showLoading();
	_trav = { dim: dim, x: Math.floor(x), z: Math.floor(z), tries: 0 };
	level.changePlayerDimension(0, dim, x + 0.5, 112, z + 0.5);
	setTimeout(_travelLandingTick, 500);
}

function _travelLandingTick() {
	if (!_trav) { _travelGuard = false; ui.hideLoading(); return; }
	var t = _trav;
	if (level.getCurrentDimension() != t.dim) {
		if (++t.tries > 180) { ui.hideLoading(); _trav = null; _travelGuard = false; return; }
		setTimeout(_travelLandingTick, 500);
		return;
	}
	// 已在新维度:引擎已按需生成区块,找地面落点。
	var y = findLandY(t.x, t.z);
	if (y < 0) {
		if (t.dim == AETHER_DIM) y = makeWelcomeIsland(t.x, t.z);
		else {
			y = findLandYNear(t.x, t.z);
			if (y < 0) { t.x = 0; t.z = 0; y = findLandY(0, 0); }
			if (y < 0) y = 64;
		}
	}
	player.teleport(t.x + 0.5, y, t.z + 0.5);
	ui.hideLoading();
	level.saveDimensionState();
	var msg = (t.dim == AETHER_DIM) ? "已进入天境。" : "已回到主世界。";
	var d = t.dim;
	_trav = null;
	_travelGuard = false;
	player.sendMessage(msg);
	modLog("[天境] travelTo 完成 dim=" + d + " @ " + t.x + "," + y + "," + t.z);
}

function onJoinWorld() {
	ui.hideLoading();
	// 加入者(joiner)的维度由服务器决定,不做任何恢复/自动传送。
	if (typeof level.isHost !== "function" || !level.isHost()) {
		modLog("[天境] onJoinWorld: joiner,维度由服务器管理");
		return;
	}
	if (_travelGuard) {
		modLog("[天境] onJoinWorld: 传送进行中,跳过");
		return;
	}
	var now = level.getTime();
	if (now - _lastJoinTick < 30) return;
	_lastJoinTick = now;
	setTimeout(function() { restoreSavedState(); }, 1000);
}

function restoreSavedState() {
	if (typeof level.isHost !== "function" || !level.isHost()) return;
	if (_travelGuard) return;
	var savedDim = parseInt(level.getModState("saved_dim"), 10);
	if (savedDim == AETHER_DIM) {
		var sx = readSavedDim(AETHER_DIM, "x", "saved_x");
		var sz = readSavedDim(AETHER_DIM, "z", "saved_z");
		var sy = readSavedDim(AETHER_DIM, "y", "saved_y");
		if (isNaN(sx)) { sx = 0; sz = 0; }
		if (!isNaN(sy) && sy > 0 && sy < 200) {
			// 有记录的坐标:直接切到天域该点。
			ui.hideLoading();
			level.changePlayerDimension(0, AETHER_DIM, sx + 0.5, sy, sz + 0.5);
			setTimeout(function(){ ui.hideLoading(); }, 800);
			modLog("[天境] restoreSavedState: 回天域 (" + sx + "," + sy + "," + sz + ")");
		} else {
			travelTo(AETHER_DIM, sx, sz, 5);
		}
	} else {
		// 默认主世界
		if (level.getCurrentDimension() != 0) {
			var mx = readSavedDim(0, "x", "");
			var mz = readSavedDim(0, "z", "");
			var my = readSavedDim(0, "y", "");
			if (isNaN(mx)) { mx = 8; mz = 8; }
			if (isNaN(my) || my <= 0 || my >= 200) my = 64;
			ui.hideLoading();
			level.changePlayerDimension(0, 0, mx + 0.5, my, mz + 0.5);
			setTimeout(function(){ ui.hideLoading(); }, 800);
			modLog("[天境] restoreSavedState: 回主世界 (" + mx + "," + my + "," + mz + ")");
		}
	}
}

function onLeaveWorld() {
	_trav = null;
	_travelGuard = false;
	ui.hideLoading();
}
