# ESP8266 IoT room thermometer

Arduino/ESP8266 code for an Internet of Things (IoT) room thermometer.

More information [on the blog](https://mansfield-devine.com/speculatrix/category/projects/iot-thermometer/).

I've posted this code following a request. But it's unlikely you'll be able to get it to work without some additional effort. It has peculiarities relating to my own environment. So it's posted here for perusing, elucidation and possible amusement rather than as a project you can simply download and run.

For example, this sensor is designed to communicate with a REST API on my intranet server via HTTP, so you'll need to set that up yourself, or change the code to suit whatever arrangements you have.

Also, because I use a number of custom fonts with the TFT_eSPI library, don't be surprised if the display doesn't run right out of the gate. You may need to do considerable tweaking. I'm afraid you're on your own with that.
