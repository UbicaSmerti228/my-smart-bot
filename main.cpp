#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <vector>

using namespace geode::prelude;

// --- ДВИЖОК ---
struct MacroAction {
    double frame;
    int button;
    bool player2;
    bool push;
};

class MacroEngine {
public:
    static MacroEngine* get() {
        static MacroEngine instance;
        return &instance;
    }
    
    bool isRecording = false;
    bool isMagnetMode = false;
    std::vector<MacroAction> actions;
    
    struct DelayedAction {
        double targetFrame;
        int button;
        bool player2;
        bool push;
    };
    std::vector<DelayedAction> queue;

    void resetQueue() { queue.clear(); }
};

// --- УПРАВЛЕНИЕ КЛАВИШАМИ (F1, F2, F3) ---
class $modify(MyKeyboard, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat) {
        // Сначала даем игре обработать клавишу
        if (CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat)) return true;
        
        // Если клавиша нажата (а не отпущена) и мы находимся в уровне (PlayLayer существует)
        if (down && !repeat && PlayLayer::get()) {
            auto engine = MacroEngine::get();
            
            // F1: ЗАПИСЬ
            if (key == KEY_F1) {
                engine->isRecording = !engine->isRecording;
                if (engine->isRecording) {
                    engine->actions.clear(); // Новая запись стирает старую
                    Notification::create("Recording Started...", NotificationIcon::Warning)->show();
                } else {
                    Notification::create("Recording Saved!", NotificationIcon::Success)->show();
                }
                return true;
            }

            // F2: РЕЖИМ БОТА (МАГНИТ)
            if (key == KEY_F2) {
                engine->isMagnetMode = !engine->isMagnetMode;
                if (engine->isMagnetMode) {
                    Notification::create("Bot Mode: ON", NotificationIcon::Success)->show();
                } else {
                    Notification::create("Bot Mode: OFF", NotificationIcon::Error)->show();
                }
                return true;
            }

            // F3: ОЧИСТКА
            if (key == KEY_F3) {
                engine->actions.clear();
                engine->queue.clear();
                engine->isRecording = false;
                engine->isMagnetMode = false;
                Notification::create("Macro Cleared", NotificationIcon::Info)->show();
                return true;
            }
        }
        return false;
    }
};

// --- ИГРОВАЯ ЛОГИКА (Та же самая, надежная) ---
class $modify(MyPlayLayer, PlayLayer) {
    void update(float dt) {
        PlayLayer::update(dt);
        auto engine = MacroEngine::get();

        if (engine->queue.empty()) return;

        double currentFrame = this->m_gameState.m_levelTime * 240.0;
        auto it = engine->queue.begin();
        while (it != engine->queue.end()) {
            if (currentFrame >= it->targetFrame) {
                if (it->push) 
                    this->m_player1->pushButton(static_cast<PlayerButton>(it->button));
                else 
                    this->m_player1->releaseButton(static_cast<PlayerButton>(it->button));
                
                it = engine->queue.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool init(LevelSettingsObject* s) {
        MacroEngine::get()->resetQueue();
        return PlayLayer::init(s);
    }
};

class $modify(MyPlayer, PlayerObject) {
    void pushButton(PlayerButton btn) {
        auto engine = MacroEngine::get();
        auto pl = PlayLayer::get();

        if (!pl) { PlayerObject::pushButton(btn); return; }

        double currentFrame = pl->m_gameState.m_levelTime * 240.0;
        bool isP2 = (this == pl->m_player2);

        if (engine->isRecording) {
            engine->actions.push_back({currentFrame, (int)btn, isP2, true});
        } 
        else if (engine->isMagnetMode && !engine->actions.empty()) {
            for (auto& act : engine->actions) {
                if (act.push && act.button == (int)btn && act.player2 == isP2) {
                    double diff = act.frame - currentFrame;
                    // Если нажали рано (до 0.2 сек), бот задержит клик до идеала
                    if (diff > 0.1 && diff < 50.0) { 
                        engine->queue.push_back({act.frame, (int)btn, isP2, true});
                        return; // Блокируем твой клик, бот нажмет сам
                    }
                }
            }
        }
        PlayerObject::pushButton(btn);
    }

    void releaseButton(PlayerButton btn) {
        auto engine = MacroEngine::get();
        auto pl = PlayLayer::get();

        if (pl && engine->isRecording) {
            double currentFrame = pl->m_gameState.m_levelTime * 240.0;
            engine->actions.push_back({currentFrame, (int)btn, (this == pl->m_player2), false});
        }
        PlayerObject::releaseButton(btn);
    }
};
