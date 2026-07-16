
physics.start()

local ground = display.newRect(400, 560, 800, 40)
ground:setFillColor(0.2, 0.8, 0.2) 

physics.addBody(ground, "static", { friction = 0.5, bounce = 0.1 })

local leftWall = display.newRect(10, 300, 20, 600)
leftWall:setFillColor(0.5, 0.5, 0.5) -- Серый цвет
physics.addBody(leftWall, "static", { friction = 0.1 })

local rightWall = display.newRect(790, 300, 20, 600)
rightWall:setFillColor(0.5, 0.5, 0.5)
physics.addBody(rightWall, "static", { friction = 0.1 })

local superBall = display.newCircle(200, 100, 25)
superBall:setFillColor(1.0, 0.1, 0.1)

physics.addBody(superBall, "dynamic", { bounce = 0.95, friction = 0.1 })

local heavyBox = display.newRect(400, 150, 60, 60)
heavyBox:setFillColor(0.1, 0.1, 1.0)

physics.addBody(heavyBox, "dynamic", { bounce = 0.1, friction = 0.8 })

for i = 1, 10 do
    local xPosition = 250 + (i * 30)
    local yPosition = 50 - (i * 40)
    
    local drop = display.newCircle(xPosition, yPosition, 12)

    drop:setFillColor(1.0 - (i * 0.08), 0.2, 0.1 + (i * 0.08))
    
    local randomBounce = 0.4 + (i * 0.05)
    physics.addBody(drop, "dynamic", { bounce = randomBounce, friction = 0.3 })
end

