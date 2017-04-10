Weewx mqtt driver (MQTT message as weather station)

The driver needs the paho-mqtt python package.

It may by installed by pip (https://pypi.python.org/pypi/paho-mqtt/1.2)


It is recommanded to add loop_on_init=true  in weewx.conf

In case network is not ready at weewx startup, driver will crash and weewx will stop.
With loop_on_init=true,  weewx will try again 1 minute later to load the driver.
