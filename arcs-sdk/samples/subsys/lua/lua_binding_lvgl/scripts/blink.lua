-- blink.lua
-- Toggle LED on PB9 (arcs_evb board)
--
-- Usage: copy to SD card, then run in shell:
--   luarun blink.lua

local gpio = lisa.gpio
local dev = lisa.device("gpiob")

local LED_PIN = 9

-- Configure PB9 as output, initial low
local ret = gpio.configure(dev, LED_PIN,
    gpio.LISA_GPIO_OUTPUT | gpio.LISA_GPIO_OUTPUT_INIT_LOW)
if ret ~= 0 then
    print("GPIO configure failed: " .. ret)
    return
end

print("Blinking LED on PB9 ... (Ctrl-C to stop)")

local led_on = true
for i = 1, 20 do
    if led_on then
        gpio.write_pin(dev, LED_PIN, gpio.LISA_GPIO_HIGH)
        print("LED ON")
    else
        gpio.write_pin(dev, LED_PIN, gpio.LISA_GPIO_LOW)
        print("LED OFF")
    end
    led_on = not led_on
    lisa.msleep(500)
end

-- Turn off LED when done
gpio.write_pin(dev, LED_PIN, gpio.LISA_GPIO_LOW)
print("Done.")
