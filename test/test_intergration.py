import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    temp = float(msg.payload.decode())
    print(f"Nhận nhiệt độ: {temp}")
    assert 20.0 <= temp <= 50.0, "Nhiệt độ ngoài ngưỡng!"

client = mqtt.Client()
client.connect('EMQX_BROKER_IP', 1883)
client.subscribe('edge/sensor/temperature')
client.on_message = on_message
print("Chờ dữ liệu từ ESP32...")
client.loop_forever()