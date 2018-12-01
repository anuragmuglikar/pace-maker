 
    wifi.set_credentials("anurag", "test1234");
    wifi.connect();
    const char* hostname = "35.196.225.7";
    int port = 1883;
    char* topic = "cis541/hw-mqtt/26013f37-13006018ae2a8ca752c368e3f5001e81/data";
    char* rTopic = "cis541/hw-mqtt/26013f37-13006018ae2a8ca752c368e3f5001e81/echo";
    MQTTNetwork network(&wifi);
    
    int rc = network.connect(hostname, port);
    if (rc == 0){
        pc.printf("TCP connection succesful. -> %d\r\n", rc);
    }else{
        pc.printf("Failed to connect to TCP Socket -> %d", rc);
    }
    
    MQTTNetwork mqttNetwork(network);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    //const char* hostname = "m2m.eclipse.org";
    //int port = 1883;
    pc.printf("Connecting to %s:%d\r\n", hostname, port);
    rc = mqttNetwork.connect(hostname, port);
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "26013f37-13006018ae2a8ca752c368e3f5001e81";
    data.username.cstring = "mbed";
    data.password.cstring = "homework";
    if ((rc = client.connect(data)) != 0)
        pc.printf("rc from MQTT connect is %d\r\n", rc);
    MQTT::Message message;
    
        int random_payload = rand() % 100 + 1;
        char buf[100];
        sprintf(buf, "%d", random_payload);
        message.qos = MQTT::QOS1;
        message.retained = true;
        message.dup = false;
        message.payload = (void*)buf;
        message.payloadlen = strlen(buf)+1;
        timer.start();
        rc = client.publish(topic, message);