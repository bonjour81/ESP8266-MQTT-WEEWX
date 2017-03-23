Weewx mqtt driver (MQTT message as weather station)

It is recommanded to add loop_on_init=true  in weewx.conf

In case network is not ready at weewx startup, driver will crash and weewx will stop.
With loop_on_init=true,  weewx will try again 1 minute later to load the driver.

