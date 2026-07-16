#include <iostream>
#include <vector>
#include <memory>
#include <string>

// Подключаем Raylib
#include "raylib.h"

#if defined(PLATFORM_ANDROID)
    #include <android/log.h>
    #define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "Luna2D", __VA_ARGS__))
#else
    #define LOGI(...) printf(__VA_ARGS__); printf("\n")
#endif

// Подключаем Box2D (Физика)
#include "box2d/box2d.h"

// Подключаем LuaJIT напрямую через C-API для максимальной скорости
extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

// ==========================================
// 1. АРХИТЕКТУРА ИГРОВЫХ ОБЪЕКТОВ
// ==========================================

enum ObjectType { RECTANGLE, CIRCLE };

struct DisplayObject {
    ObjectType type;
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    float radius = 0;
    Color color = RED;
    
    // Ссылка на физическое тело Box2D
    b2Body* physicsBody = nullptr;

    void draw() {
        if (type == RECTANGLE) {
            // Рисуем прямоугольник из центра
            DrawRectanglePro({ x, y, width, height }, { width / 2, height / 2 }, 0, color);
        } else if (type == CIRCLE) {
            // Рисуем круг
            DrawCircle(x, y, radius, color);
        }
    }
};

// Глобальный список объектов для рендеринга
std::vector<std::shared_ptr<DisplayObject>> sceneObjects;

// Физический мир Box2D (гравитация вниз 9.8)
std::unique_ptr<b2World> physicsWorld = std::make_unique<b2World>(b2Vec2(0.0f, 9.8f));
bool physicsRunning = false;
const float PIXELS_PER_METER = 30.0f; // Масштаб физики

// ==========================================
// 2. ФУНКЦИИ ДЛЯ LUA (LUA API)
// ==========================================

// Системный хелпер для получения объектов из Lua
DisplayObject* toDisplayObject(lua_State* L, int index) {
    lua_getfield(L, index, "id");
    int id = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return sceneObjects[id].get();
}

// display.newRect(x, y, width, height)
static int l_newRect(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);

    auto obj = std::make_shared<DisplayObject>();
    obj->type = RECTANGLE;
    obj->x = x; obj->y = y; obj->width = w; obj->height = h;
    obj->color = RED;

    sceneObjects.push_back(obj);
    int objId = sceneObjects.size() - 1;

    // Возвращаем Lua-таблицу (объект)
    lua_newtable(L);
    lua_pushinteger(L, objId);
    lua_setfield(L, -2, "id");

    // Добавляем метод setFillColor в объект
    lua_pushcfunction(L, [](lua_State* L) -> int {
        DisplayObject* o = toDisplayObject(L, 1);
        float r = (float)luaL_checknumber(L, 2);
        float g = (float)luaL_checknumber(L, 3);
        float b = (float)luaL_checknumber(L, 4);
        o->color = Color{ (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
        return 0;
    });
    lua_setfield(L, -2, "setFillColor");

    return 1; 
}

// display.newCircle(x, y, radius)
static int l_newCircle(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float r = (float)luaL_checknumber(L, 3);

    auto obj = std::make_shared<DisplayObject>();
    obj->type = CIRCLE;
    obj->x = x; obj->y = y; obj->radius = r;
    obj->color = BLUE;

    sceneObjects.push_back(obj);
    int objId = sceneObjects.size() - 1;

    lua_newtable(L);
    lua_pushinteger(L, objId);
    lua_setfield(L, -2, "id");

    // Метод setFillColor для круга
    lua_pushcfunction(L, [](lua_State* L) -> int {
        DisplayObject* o = toDisplayObject(L, 1);
        float r = (float)luaL_checknumber(L, 2);
        float g = (float)luaL_checknumber(L, 3);
        float b = (float)luaL_checknumber(L, 4);
        o->color = Color{ (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
        return 0;
    });
    lua_setfield(L, -2, "setFillColor");

    return 1;
}

// physics.start()
static int l_physicsStart(lua_State* L) {
    physicsRunning = true;
    LOGI("Physics started.");
    return 0;
}

// physics.addBody(object, type, params)
static int l_physicsAddBody(lua_State* L) {
    DisplayObject* obj = toDisplayObject(L, 1);
    std::string bodyType = luaL_checkstring(L, 2);

    // Парсим параметры (bounce, friction)
    float bounce = 0.0f;
    float friction = 0.3f;
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "bounce");
        if (!lua_isnil(L, -1)) bounce = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 3, "friction");
        if (!lua_isnil(L, -1)) friction = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    // Создаем тело в Box2D
    b2BodyDef bodyDef;
    if (bodyType == "dynamic") {
        bodyDef.type = b2_dynamicBody;
    } else {
        bodyDef.type = b2_staticBody;
    }
    bodyDef.position.Set(obj->x / PIXELS_PER_METER, obj->y / PIXELS_PER_METER);

    b2Body* body = physicsWorld->CreateBody(&bodyDef);

    // Задаем форму
    if (obj->type == RECTANGLE) {
        b2PolygonShape box;
        box.SetAsBox((obj->width / 2.0f) / PIXELS_PER_METER, (obj->height / 2.0f) / PIXELS_PER_METER);
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &box;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = friction;
        fixtureDef.restitution = bounce;
        body->CreateFixture(&fixtureDef);
    } else if (obj->type == CIRCLE) {
        b2CircleShape circle;
        circle.m_radius = obj->radius / PIXELS_PER_METER;

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &circle;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = friction;
        fixtureDef.restitution = bounce;
        body->CreateFixture(&fixtureDef);
    }

    obj->physicsBody = body;
    return 0;
}

// ==========================================
// 3. СВЯЗЫВАНИЕ И СТАРТ ДВИЖКА
// ==========================================

void registerLuaAPI(lua_State* L) {
    // Регистрация модуля display
    lua_newtable(L);
    lua_pushcfunction(L, l_newRect);
    lua_setfield(L, -2, "newRect");
    lua_pushcfunction(L, l_newCircle);
    lua_setfield(L, -2, "newCircle");
    lua_setglobal(L, "display");

    // Регистрация модуля physics
    lua_newtable(L);
    lua_pushcfunction(L, l_physicsStart);
    lua_setfield(L, -2, "start");
    lua_pushcfunction(L, l_physicsAddBody);
    lua_setfield(L, -2, "addBody");
    lua_setglobal(L, "physics");
}

// Точка входа, совместимая с Android и ПК
#if defined(PLATFORM_ANDROID)
void android_main(struct android_app* app) {
    // На Android Raylib инициализируется внутри своего системного цикла
#else
int main() {
#endif

    // Инициализация окна
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Luna2D Engine");
    SetTargetFPS(60);

    // Запуск LuaJIT
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    registerLuaAPI(L);

    // Загружаем скрипт игры
    // На Android файлы читаются из папки assets внутри APK, на ПК - локально.
    #if defined(PLATFORM_ANDROID)
        // Для простоты на Android грузим встроенный тестовый код, если файл не найден
        if (luaL_dofile(L, "assets/main.lua") != 0) {
            LOGI("Failed to load main.lua, using fallback script.");
            const char* fallback = 
                "physics.start(); "
                "local g = display.newRect(400, 550, 800, 50); g:setFillColor(0, 1, 0); physics.addBody(g, 'static'); "
                "local b = display.newCircle(400, 100, 30); b:setFillColor(1, 0, 0); physics.addBody(b, 'dynamic', {bounce = 0.7});";
            luaL_dostring(L, fallback);
        }
    #else
        if (luaL_dofile(L, "assets/main.lua") != 0) {
            std::cout << "Error loading main.lua: " << lua_tostring(L, -1) << std::endl;
        }
    #endif

    // Главный игровой цикл
    while (!WindowShouldClose()) {
        
        // 1. Обновление физики Box2D
        if (physicsRunning) {
            // Шаг физики: 1/60 секунды, 6 итераций скорости, 2 итерации положения
            physicsWorld->Step(1.0f / 60.0f, 6, 2);

            // Переносим координаты из физического мира в графику
            for (auto& obj : sceneObjects) {
                if (obj->physicsBody != nullptr) {
                    b2Vec2 pos = obj->physicsBody->GetPosition();
                    obj->x = pos.x * PIXELS_PER_METER;
                    obj->y = pos.y * PIXELS_PER_METER;
                }
            }
        }

        // 2. Отрисовка кадра
        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (auto& obj : sceneObjects) {
            obj->draw();
        }

        DrawFPS(10, 10);
        EndDrawing();
    }

    // Очистка ресурсов
    lua_close(L);
    CloseWindow();

#if !defined(PLATFORM_ANDROID)
    return 0;
#endif
}
