MS5611 to MQTT python script


Current script use basic altitude compensation to calculate barometer value.

More evolution may come to use more accurate calculations.


You can execute this scrip every 5 minutes by adding a line in /etc/crontab:

*/5 * *   *   *   username  /home/username/ms5611

Dont forget to make the script executable (chmod +x ms5611)
