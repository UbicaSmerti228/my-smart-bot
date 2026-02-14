#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <geode.imgui/imgui.hpp>
#include <vector>

using namespace geode::prelude;

// --- СТРУКТУРА ДАННЫХ ---
struct MacroAction {
    double frame;
    int button;
    bool player2;
    bool push;
};

// --- ДВИЖОК МОДА ---
class MacroEngine {
public:
    static MacroEngine* get() {
        static MacroEngine instance;
        return &instance;
    }

    bool isRecording = false;
    bool isMagnetMode = false;
    bool showGui = false;
    
    std::vector<MacroAction> actions; // Записанный макрос
    
    struct DelayedAction {
        double targetFrame;
        int button;
        bool player2;
        bool push;
    };
    std::vector<DelayedAction> queue; // Очередь для "Магнита"

    void resetQueue() { queue.clear(); }
};

// --- ИНТЕРФЕЙС (ImGui) ---
$execute {
    ImGuiCustom::addRenderCallback([]() {
        auto engine = MacroEngine::get();
        if (!engine->showGui) return;

        ImGui::Begin("Smart Assistant by Yasya's Friend", &engine->showGui);
        
        if (engine->isRecording) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "STATUS: RECORDING...");
        } else {
            ImGui::Text("STATUS: IDLE");
        }

        if (ImGui::Button(engine->isRecording ? "Stop Recording" : "Start Recording", ImVec2(-1, 0))) {
            engine->isRecording = !engine->isRecording;
        }

        ImGui::Checkbox("Enable Magnet Correction", &engine->isMagnetMode);
        
        if (ImGui::Button("Clear Current Macro", ImVec2(-1, 0))) {
            engine->actions.clear();
        }

        ImGui::Separator();
        ImGui::Text("Saved Actions: %d", (int)engine->actions.size());
        ImGui::Text("Current Queue: %d", (int)engine->queue.size());
        
        ImGui::End();
    });
}

// --- ХУКИ (ГЛАВНАЯ ЛОГИКА) ---

// 1. Кнопка в главном меню
class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        auto menu = this->getChildByID("side-menu");
        auto spr = CircleButtonSprite::createWithSpriteFrameName("geode.loader/geode-logo-outline.png", 0.7f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MyMenuLayer::onOpenSmartBot));
        if (menu) {
            menu->addChild(btn);
            menu->updateLayout();
        }
        return true;
    }
    void onOpenSmartBot(CCObject*) {
        MacroEngine::get()->showGui = !MacroEngine::get()->showGui;
    }
};

// 2. Обработка времени и коррекции (совместимо со спидхаком)
class $modify(MyPlayLayer, PlayLayer) {
    void update(float dt) {
        PlayLayer::update(dt);
        auto engine = MacroEngine::get();

        if (engine->queue.empty()) return;

        // Используем levelTime * 240 для определения "игрового кадра"
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

// 3. Запись и перехват кликов
class $modify(MyPlayer, PlayerObject) {
    void pushButton(PlayerButton btn) {
        auto engine = MacroEngine::get();
        auto pl = PlayLayer::get();

        if (!pl) {
            PlayerObject::pushButton(btn);
            return;
        }

        double currentFrame = pl->m_gameState.m_levelTime * 240.0;
        bool isP2 = (this == pl->m_player2);

        // Если запись включена - сохраняем кадр
        if (engine->isRecording) {
            engine->actions.push_back({currentFrame, (int)btn, isP2, true});
        } 
        // Если "Магнит" включен и мы НЕ записываем
        else if (engine->isMagnetMode && !engine->actions.empty()) {
            for (auto& act : engine->actions) {
                if (act.push && act.button == (int)btn && act.player2 == isP2) {
                    double diff = act.frame - currentFrame;
                    // Если нажали слишком рано (до 8 кадров), откладываем клик
                    if (diff > 0.1 && diff < 8.0) {
                        engine->queue.push_back({act.frame, (int)btn, isP2, true});
                        return; // Блокируем раннее нажатие
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