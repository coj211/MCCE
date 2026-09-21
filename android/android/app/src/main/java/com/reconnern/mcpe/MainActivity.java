package com.reconnern.mcpe;

import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Insets;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Android 入口 Activity。
 *
 * 主体逻辑全在 native 侧（libminecraftpe.so），这里只提供 native 拿不到的
 * 平台能力：软键盘文本输入、震动、设备型号、保持屏幕常亮等。
 *
 * NativeActivity 会 dlopen(android.app.lib_name) 并调用 ANativeActivity_onCreate
 * （由 NDK 的 native_app_glue 提供），我们只在这些 Java 辅助方法里做桥接。
 */
public class MainActivity extends NativeActivity {
    private static final String TAG = "MinecraftPE";

    private static MainActivity sInstance;

    /** Java 侧的隐藏输入框：NativeActivity 默认没有 InputConnection，
     *  软键盘敲出来的字符拿不到，所以挂一个 1x1 的 EditText 专门吃文本。 */
    private EditText mEditText;
    private boolean mSuppressText;

    static MainActivity get() {
        return sInstance;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sInstance = this;

        // 诊断 + 保险：NativeActivity 理论上已经 loadLibrary 过，但 JNI 静态注册
        // 找不到符号时会报 “No implementation found”（曾让软键盘、震动、右
        // 侧 inset 全部静默失效）。这里显式再加载一次（幂等），并把 native
        // 库目录打出来，方便确认 ABI 与符号究竟能不能用。
        try {
            System.loadLibrary("minecraftpe");
            Log.i(TAG, "loadLibrary(minecraftpe) ok; nativeLibDir="
                    + getApplicationInfo().nativeLibraryDir);
        } catch (Throwable t) {
            Log.e(TAG, "loadLibrary(minecraftpe) failed", t);
        }
        // 游戏是全屏交互，屏幕常亮
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        applyImmersiveFullscreen();
        // 窗口布局完成后才能拿到 insets（沉浸式生效后右侧通常是 0）
        getWindow().getDecorView().post(new Runnable() {
            @Override
            public void run() {
                pushSystemInsetsToNative();
            }
        });
    }

    /**
     * 把“系统接管、不向应用派发触摸”的右侧宽度（物理像素）推给 native。
     *
     * 屏幕右边缘那条导航栏/边缘手势区属于系统：应用能画上去（所以看得见），
     * 但那里的触摸不派发给应用（所以点不着）。native 侧把这块当“屏幕外”，
     * 贴右边缘的 HUD / 开关就不会画到摸不到的地方；沉浸式全屏生效时这里是 0，
     * 于是右上角按钮就回到最右上角。
     */
    private void pushSystemInsetsToNative() {
        // 沉浸式全屏（我们在 applyImmersiveFullscreen 里设了 HIDE_NAVIGATION）下
        // 系统栏是一直隐藏的，触摸能打到屏幕最右边 —— 右侧 inset 必须是 0。
        //
        // 为什么不能信 windowInsets：ColorOS 即使隐藏了导航栏，
        // getInsets(systemGestures) / isVisible(navigationBars) 仍会报出
        // 132px。一旦采信，UI 就按「屏幕变窄」重新布局，而 GL 画面仍是全屏画
        // 的 —— 按钮画在右、命中区在左，设置里的开关、世界列表的删除按钮全部
        // 点不到（观感就是“整体偏右”）。
        final int uiFlags = getWindow().getDecorView().getSystemUiVisibility();
        if ((uiFlags & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) != 0) {
            try {
                nativeSetRightInset(0);
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "nativeSetRightInset not available", e);
            }
            return;
        }

        int right = 0;
        try {
            WindowInsets wi = getWindow().getDecorView().getRootWindowInsets();
            if (wi != null) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    // 只有系统栏「正在显示」时，右侧那一条才真的会吃掉触摸。
                    // 沉浸式全屏下系统栏是隐藏的、触摸能一直打到最右边，这时必须
                    // 推 0：否则 UI 会按「屏幕变窄」重新布局，而 GL 画面仍是全屏
                    // 画的 —— 按钮画在右、命中区在左，右侧的开关/删除按钮就全部
                    // 点不到了（观感就是“整体偏右”）。
                    // 也不能算 systemGestures()：屏幕边缘的返回手势区照样会把触摸
                    // 派发给应用，算进去同样是错的。
                    if (wi.isVisible(WindowInsets.Type.navigationBars())) {
                        right = wi.getInsets(WindowInsets.Type.navigationBars()).right;
                    }
                } else {
                    final int flags = getWindow().getDecorView().getSystemUiVisibility();
                    if ((flags & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0) {
                        right = Math.max(wi.getStableInsetRight(), wi.getSystemWindowInsetRight());
                    }
                }
            }
        } catch (Throwable t) {
            Log.w(TAG, "pushSystemInsetsToNative failed", t);
        }
        try {
            nativeSetRightInset(right);
        } catch (UnsatisfiedLinkError e) {
            // 不该发生（符号在 libminecraftpe.so 里）；万一 .so 是旧的，
            // 也只是退回"不扣右侧 inset"，不要连累整个游戏崩溃。
            Log.w(TAG, "nativeSetRightInset not available", e);
        }
    }

    /** 系统栏（尤其是导航栏）被系统或用户手势重新显示时，抢回沉浸式全屏。 */
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveFullscreen();
            // 系统栏收放会让 insets 变化，重新推一次
            pushSystemInsetsToNative();
        }
    }

    /**
     * 沉浸式全屏：隐藏状态栏 + 导航栏，把整屏交还给应用。
     *
     * 为什么需要：横屏时屏幕右侧那条半透明导航栏既会盖住游戏画面，又属于
     * 系统——那块区域的触摸不会派发给应用，于是贴屏幕最右边的 HUD / 设置开关
     * 就变成「看得见、点不着」（右上角聊天按钮以前就是这个毛病）。隐藏导航栏
     * 后触摸能到达右侧边缘。
     *
     * BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE：从边缘滑一下会临时显示系统栏
     * （这一下被系统栏吃掉），再滑才是系统手势。
     */
    private void applyImmersiveFullscreen() {
        try {
            // 旧 flag 在部分厂商 ROM（含 ColorOS）上才真正生效，与 API 30+ 的
            // WindowInsetsController 双写，两侧都兼顾。
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {   // API 30+
                WindowInsetsController c = getWindow().getInsetsController();
                if (c != null) {
                    c.hide(WindowInsets.Type.statusBars()
                            | WindowInsets.Type.navigationBars());
                    c.setSystemBarsBehavior(
                            WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                }
            }
        } catch (Throwable t) {
            Log.w(TAG, "applyImmersiveFullscreen failed", t);
        }
    }

    @Override
    protected void onDestroy() {
        sInstance = null;
        super.onDestroy();
    }

    // ── 软键盘 ────────────────────────────────────────────────────────────

    /** 由 native 调用：弹出软键盘，带上当前文本。 */
    public void showSoftKeyboard(final String initialText, final int maxLength) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ensureEditText(maxLength);
                mSuppressText = true;
                mEditText.setText(initialText == null ? "" : initialText);
                mEditText.setSelection(mEditText.getText().length());
                mSuppressText = false;
                mEditText.setVisibility(View.VISIBLE);
                mEditText.requestFocus();
                // 焦点要等这一帧布局完成才真正落到 EditText 上；紧跟着调
                // showSoftInput 在不少 ROM（ColorOS/MIUI 等）上会被直接忽略。
                // 延一小会儿再弹，并在日志里留证据。
                mEditText.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        InputMethodManager imm = (InputMethodManager)
                                getSystemService(Context.INPUT_METHOD_SERVICE);
                        if (imm == null) {
                            Log.e(TAG, "showSoftKeyboard: no InputMethodManager");
                            return;
                        }
                        imm.showSoftInput(mEditText, InputMethodManager.SHOW_FORCED);
                        Log.i(TAG, "showSoftKeyboard: shown=" + mEditText.isShown()
                                + " focused=" + mEditText.isFocused()
                                + " accepting=" + imm.isAcceptingText());
                    }
                }, 120);
            }
        });
    }

    /**
     * 由 native 调用：把输入法重新叫出来，但不改动 EditText 里的文本。
     * 安卓在点击「输入框以外」的地方时会自动收起 IME，聊天屏里点一下画面就没法
     * 继续打字了 —— 所以每次点击后由 native 再叫一次。
     * 不能复用 showSoftKeyboard()：那个会 setText() 重置，会抹掉正在拼的拼音。
     */
    public void reshowSoftKeyboard() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mEditText == null || mEditText.getVisibility() != View.VISIBLE) return;
                mEditText.requestFocus();
                InputMethodManager imm = (InputMethodManager)
                        getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.showSoftInput(mEditText, InputMethodManager.SHOW_FORCED);
                }
            }
        });
    }

    /** 由 native 调用：收起软键盘，返回当前文本。 */
    public String hideSoftKeyboard() {
        final String[] out = new String[] { "" };
        final Object lock = new Object();
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mEditText != null) {
                    out[0] = mEditText.getText().toString();
                    InputMethodManager imm = (InputMethodManager)
                            getSystemService(Context.INPUT_METHOD_SERVICE);
                    if (imm != null) {
                        imm.hideSoftInputFromWindow(mEditText.getWindowToken(), 0);
                    }
                    mEditText.setVisibility(View.GONE);
                    mEditText.clearFocus();
                }
                synchronized (lock) { lock.notifyAll(); }
            }
        });
        synchronized (lock) {
            try { lock.wait(500); } catch (InterruptedException ignored) {}
        }
        return out[0];
    }

    private void ensureEditText(int maxLength) {
        if (mEditText != null) {
            return;
        }
        mEditText = new EditText(this);
        mEditText.setSingleLine(true);
        mEditText.setImeOptions(EditorInfo.IME_ACTION_DONE);
        mEditText.setInputType(EditorInfo.TYPE_CLASS_TEXT
                | EditorInfo.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        mEditText.setBackgroundColor(0x00000000);
        mEditText.setTextColor(0x00000000);
        // 1x1 的透明输入框，不影响渲染（游戏画面由 native GLSurface 绘制）
        ViewGroup.LayoutParams lp = new ViewGroup.LayoutParams(1, 1);
        addContentView(mEditText, lp);
        mEditText.setVisibility(View.GONE);

        // 软键盘的「完成」键不会产生 AKEYCODE_ENTER（native 收不到回车），
        // 所以这里往文本末尾补一个换行；native 侧会剥掉它并派发一次 KEY_RETURN
        // （聊天屏、告示牌这些屏就是靠回车提交的）。
        mEditText.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                final boolean done = (actionId == EditorInfo.IME_ACTION_DONE
                        || actionId == EditorInfo.IME_ACTION_SEND
                        || actionId == EditorInfo.IME_ACTION_GO);
                final boolean enter = (event != null
                        && event.getKeyCode() == KeyEvent.KEYCODE_ENTER
                        && event.getAction() == KeyEvent.ACTION_DOWN);
                if (done || enter) {
                    if (mEditText != null) mEditText.append("\n");
                    return true;
                }
                return false;
            }
        });

        final int max = maxLength > 0 ? maxLength : 256;
        mEditText.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
            @Override public void onTextChanged(CharSequence s, int a, int b, int c) {}
            @Override
            public void afterTextChanged(Editable s) {
                if (mSuppressText) return;
                String t = s.toString();
                if (t.length() > max) {
                    t = t.substring(0, max);
                    mSuppressText = true;
                    mEditText.setText(t);
                    mEditText.setSelection(t.length());
                    mSuppressText = false;
                }
                nativeOnTextChanged(t);
                Log.i(TAG, "afterTextChanged: len=" + t.length() + " [" + t + "]");
            }
        });
    }

    // ── 震动 ──────────────────────────────────────────────────────────────

    public void vibrate(final int ms) {
        if (ms <= 0) return;
        try {
            Vibrator v = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
            if (v == null || !v.hasVibrator()) return;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                v.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE));
            } else {
                v.vibrate(ms);
            }
        } catch (Throwable t) {
            Log.w(TAG, "vibrate failed", t);
        }
    }

    // ── 设备信息 ──────────────────────────────────────────────────────────

    public String getDeviceModel() {
        String m = Build.MANUFACTURER + " " + Build.MODEL;
        return m.trim();
    }

    public String getDeviceLocale() {
        return java.util.Locale.getDefault().toString();
    }

    // native 侧实现（JNI）
    private native void nativeOnTextChanged(String text);

    /** 把「系统接管、不向应用派发触摸」的右侧宽度（物理像素）推给 native。 */
    private native void nativeSetRightInset(int px);

    private native void nativeOnModFilePicked(String path);

    // ── 模组安装：系统文件选择器 ──────────────────────────────────────────

    private static final int REQ_PICK_MOD = 1001;

    /** 由 native 调用（ModsScreen 的"安装模组"按钮）。 */
    public void pickModFile() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
                    intent.setType("*/*");
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    startActivityForResult(Intent.createChooser(intent, "选择模组 zip"), REQ_PICK_MOD);
                } catch (Throwable t) {
                    Log.e(TAG, "pickModFile failed", t);
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQ_PICK_MOD || resultCode != RESULT_OK
                || data == null || data.getData() == null) {
            return;
        }
        final Uri uri = data.getData();
        // 系统返回的是 content:// URI，native 侧（ModEngine）用 fopen 读路径，
        // 所以先复制成一个真实文件再回传。
        try {
            File dir = getExternalFilesDir(null);
            if (dir == null) dir = getFilesDir();
            File dst = new File(dir, "picked_mod.zip");
            InputStream in = getContentResolver().openInputStream(uri);
            if (in == null) return;
            OutputStream out = new FileOutputStream(dst);
            try {
                byte[] buf = new byte[16384];
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                }
                out.flush();
            } finally {
                try { in.close(); } catch (IOException ignored) {}
                try { out.close(); } catch (IOException ignored) {}
            }
            nativeOnModFilePicked(dst.getAbsolutePath());
        } catch (Throwable t) {
            Log.e(TAG, "onActivityResult copy failed", t);
        }
    }
}
