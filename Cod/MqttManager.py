import paho.mqtt.client as mqtt

broker_ip = "IP_BROKER"  # Adresa broker-ului MQTT (ex: 192.168.1.100)
broker_port = 1883
device_id = "deviceID"  # Înlocuiește cu deviceID-ul plăcuței

topic = f"commands/{device_id}"
command = "blink"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Conectat la MQTT broker!")
    else:
        print(f"Conexiune eșuată cu codul {rc}")

client = mqtt.Client()
client.on_connect = on_connect

client.connect(broker_ip, broker_port, 60)
client.loop_start()

# Trimite comanda blink
for i in range(5):
    client.publish(topic, command)
    print(f"Trimis comanda '{command}' către {topic}")

client.loop_stop()
client.disconnect()
