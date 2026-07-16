#include <iostream>
#include <vector>
#include <memory>
#include <string>

#include "raylib.h"

#if defined(PLATFORM_ANDROID)
    #include <android/log.h>
    #define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "Luna2D", __VA_ARGS__))
#else
    #define LOGI(...) printf(__VA_ARGS__); printf("\n")
#endif

// Подключаем современный Box2D v3
#include "box2d/box2d.h"

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

// === СТРУКТУРЫ ИГРОВЫХ ОБЪЕКТОВ ===
enum ObjectType { RECTANGLE, CIRCLE };

struct DisplayObject {
    ObjectType type;
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    float radius = 0;
    Color color = RED;
    
    // В Box2D v3 тела идентифицируются через легковесные структуры b2BodyId
    b2BodyId physicsBody = b2_nullBodyId;

    void draw() {
        if (type == RECTANGLE) {
            DrawRectanglePro({ x, y, width, height }, { width / 2, height / 2 }, 0, color);
        } else if (type == CIRCLE) {
            DrawCircle(x, y, radius, color);
        }
    }
};

std::vector<std::shared_ptr<DisplayObject>> sceneObjects;

// В Box2D v3 мир теперь тоже идентифицируется через b2WorldId
b2WorldId physicsWorld = b2_nullWorldId;
bool physicsRunning = false;
const float PIXELS_PER_METER = 30.0f;

// === СВЯЗУЮЩИЕ ФУНКЦИИ ДЛЯ LUA (API) ===
DisplayObject* toDisplayObject(lua_State* L, int index) {
    lua_getfield(L, index, "id");
    int id = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return sceneObjects[id].get();
}

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

    lua_newtable(L);
    lua_pushinteger(L, objId);
    lua_setfield(L, -2, "id");

    // Обернули лямбду в скобки ( ... ) чтобы макрос lua_pushcfunction не давал сбоев
    lua_pushcfunction(L, ([] (lua_State* L) -> int {
        DisplayObject* o = toDisplayObject(L, 1);
        float r = (float)luaL_checknumber(L, 2);
        float g = (float)luaL_checknumber(L, 3);
        float b = (float)luaL_checknumber(L, 4);
        o->color = Color{ (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
        return 0;
    }));
    lua_setfield(L, -2, "setFillColor");

    return 1; 
}

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

    lua_pushcfunction(L, ([] (lua_State* L) -> int {
        DisplayObject* o = toDisplayObject(L, 1);
        float r = (float)luaL_checknumber(L, 2);
        float g = (float)luaL_checknumber(L, 3);
        float b = (float)luaL_checknumber(L, 4);
        o->color = Color{ (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
        return 0;
    }));
    lua_setfield(L, -2, "setFillColor");

    return 1;
}

static int l_physicsStart(lua_State* L) {
    physicsRunning = true;
    LOGI("Physics started (Box2D v3.0).");
    return 0;
}

static int l_physicsAddBody(lua_State* L) {
    DisplayObject* obj = toDisplayObject(L, 1);
    std::string bodyType = luaL_checkstring(L, 2);

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

    // Настройка тела в Box2D v3
    b2BodyDef bodyDef = b2DefaultBodyDef();
    if (bodyType == "dynamic") {
        bodyDef.type = b2_dynamicBody;
    } else {
        bodyDef.type = b2_staticBody;
    }
    bodyDef.position = b2Vec2{ obj->x / PIXELS_PER_METER, obj->y / PIXELS_PER_METER };

    // Создаем тело в мире
    b2BodyId body = b2CreateBody(physicsWorld, &bodyDef);

    // Создаем и крепим форму к телу
    if (obj->type == RECTANGLE) {
        b2Polygon box = b2MakeBox((obj->width / 2.0f) / PIXELS_PER_METER, (obj->height / 2.0f) / PIXELS_PER_METER);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = 1.0f;
        shapeDef.friction = friction;
        shapeDef.restitution = bounce;
        b2CreatePolygonShape(body, &shapeDef, &box);
    } else if (obj->type == CIRCLE) {
        b2Circle circle;
        circle.center = b2Vec2{0.0f, 0.0f};
        circle.radius = obj->radius / PIXELS_PER_METER;
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = 1.0f;
        shapeDef.friction = friction;
        shapeDef.restitution = bounce;
        b2CreateCircleShape(body, &shapeDef, &circle);
    }

    obj->physicsBody = body;
    return 0;
}

void registerLuaAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_newRect);
    lua_setfield(L, -2, "newRect");
    lua_pushcfunction(L, l_newCircle);
    lua_setfield(L, -2, "newCircle");
    lua_setglobal(L, "display");

    lua_newtable(L);
    lua_pushcfunction(L, l_physicsStart);
    lua_setfield(L, -2, "start");
    lua_pushcfunction(L, l_physicsAddBody);
    lua_setfield(L, -2, "addBody");
    lua_setglobal(L, "physics");
}

#if defined(PLATFORM_ANDROID)
void android_main(struct android_app* app) {
#else
int main() {
#endif

    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Luna2D Engine");
    SetTargetFPS(60);

    // Инициализация мира в Box2D v3
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = b2Vec2{ 0.0f, 9.8f }; // Гравитация вниз
    physicsWorld = b2CreateWorld(&worldDef);

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    registerLuaAPI(L);

    #if defined(PLATFORM_ANDROID)
        if (luaL_dofile(L, "assets/main.lua") != 0) {
            LOGI("Failed to load assets/main.lua! Engine stopped.");
        }
    #else
        if (luaL_dofile(L, "main.lua") != 0) {
            std::cout << "Error loading main.lua: " << lua_tostring(L, -1) << std::endl;
        }
    #endif

    while (!WindowShouldClose()) {
        if (physicsRunning) {
            // В Box2D v3 шаг симуляции делается через b2World_Step
            // Рекомендуется использовать 4 суб-шага для стабильности столкновений
            b2World_Step(physicsWorld, 1.0f / 60.0f, 4);

            for (auto& obj : sceneObjects) {
                if (b2IsValid(obj->physicsBody)) {
                    b2Vec2 pos = b2Body_GetPosition(obj->physicsBody);
                    obj->x = pos.x * PIXELS_PER_METER;
                    obj->y = pos.y * PIXELS_PER_METER;
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (auto& obj : sceneObjects) {
            obj->draw();
        }

        DrawFPS(10, 10);
        EndDrawing();
    }

    // Уничтожение мира Box2D v3 при выходе
    b2DestroyWorld(physicsWorld);

    lua_close(L);
    CloseWindow();

#if !defined(PLATFORM_ANDROID)
    return 0;
#endif
}
