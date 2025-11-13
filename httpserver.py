from flask import Flask, request
from influxdb import InfluxDBClient

import struct, base64, json

app = Flask(__name__)
#client = InfluxDBClient(host='data.yoerik.com', port=8086)
client = InfluxDBClient(host='10.10.1.20', port=8086)
#client = InfluxDBClient(host='mydomain.com', port=8086, username='myuser', password='mypass' ssl=True, verify_ssl=True)

print(client.get_list_database())

client.create_database('hydroponics')
client.switch_database('hydroponics')

def record_data(rawdata):
    data = {}
    try:
        datasplit = rawdata['data'].split(', ')
        datasplit = [d.split(':') for d in datasplit]

        for i in datasplit:
            data[i[0]] = float(i[1])
    except (ValueError, IndexError):
        data['raw'] = rawdata
    json_body = [
        {
            "measurement": rawdata['event'],
            "tags": {'coreid':rawdata['coreid']},
            "fields": data
        }
    ]
    client.switch_database('hydroponics')
    client.write_points(json_body)

def record_data_particle_json(rawdata):
    data = {}
    print(rawdata)
    data = json.loads(rawdata['data'])
    print('parsed data:', data)
    json_body = [
        {
            "measurement": rawdata['event'],
            "tags": {'coreid':rawdata['coreid']},
            "fields": data
        }
    ]
    client.switch_database('hydroponics')
    client.write_points(json_body)

@app.route('/loratag', methods=['GET', 'POST'])
def process_loratag():
    data = request.json
    print(data)
    content = data['payload_fields']
    gateway = data['metadata']['gateways'][0]
    content['rssi'] = int(gateway['rssi'])
    content['snr'] = float(gateway['snr'])
    content['latitude'] = float(content.get('latitude',0.0))
    content['longitude'] = float(content.get('longitude',0.0))
    json_body = [
        {
            "measurement": 'loratag',
            "tags": {'delivery_method':'http'},
            "fields": content
        }
    ]
    client.switch_database('lora_log')
    client.write_points(json_body)

    return 'Success'

@app.route('/heliumtag', methods=['GET', 'POST'])
def process_heliumtag():
    data = request.json
    print(data)
    content = data['decoded']['payload']
    json_body = [
        {
            "measurement": 'loratag',
            "tags": {'delivery_method':'http'},
            "fields": content
        }
    ]
    client.switch_database('lora_log')
    client.write_points(json_body)

    return 'Success'

@app.route('/helium', methods=['GET', 'POST'])
def process_helium():
    content = request.json
    print(content)


    s = struct.Struct('f f i i')
    text = content['payload']
    missing_padding = len(text) % 4
    if missing_padding:
        text += b'='* (4 - missing_padding)

    lat, lon, alt, vbat = s.unpack(base64.b64decode(text))
    json_body = [
        {
            "measurement": content['dev_eui'],
            "tags": {'delivery_method':'http'},
            "fields": {'rssi':      content['hotspots'][0]['rssi'],
                       'snr':       content['hotspots'][0]['snr'],
                       'spreading': content['hotspots'][0]['spreading'],
                       'channel':   content['hotspots'][0]['channel'],
                       'name':      content['hotspots'][0]['name'],
                       'frequency': content['hotspots'][0]['frequency'],
                       'payload':   content['payload'],
                       'lat': lat,
                       'lon': lon,
                       'vbat': vbat
                       }
        }
    ]
    client.switch_database('lora_log')
    client.write_points(json_body)
    return 'Success'

@app.route('/data', methods=['GET', 'POST'])
def process_data():
    content = request.json
    record_data(content)
    print(content)
    return 'Success'

@app.route('/jsonlog', methods=['GET', 'POST'])
def process_data_json():
    content = json.loads(request.json['data'])
    json_body = [
        {
            "measurement": content.pop('measurement'),
            "tags": {'delivery_method':'http', 'coreid':request.json['coreid']},
            "fields": content
        }
    ]
    client.switch_database('room_log')
    client.write_points(json_body)

    return 'Success'

def process_log_point(point_data):
    """
    Process a single data point and return formatted InfluxDB point structure.
    
    Args:
        point_data: Dictionary with 'measurement', optional 'tags', and 'fields' or remaining data as fields
    
    Returns:
        Dictionary formatted for InfluxDB write_points
    """
    point_copy = point_data.copy() if isinstance(point_data, dict) else {}
    
    measurement = point_copy.pop('measurement')
    
    tags = {'delivery_method':'http'}
    if point_copy.get('tags'):
        tags |= point_copy.pop('tags')
    
    if point_copy.get('fields'):
        fields = point_copy.pop('fields')
    else:
        fields = point_copy
    
    return {
        "measurement": measurement,
        "tags": tags,
        "fields": fields
    }

@app.route('/logtemperatures', methods=['GET', 'POST'])
def process_temperatures():
    content = request.json
    #print(content)


    """ Input JSON:
    {
        "measurement": "onewire_temp",
        "arduino_millis": 5000,
        "values": {
            "28E429A699230B52": 22,
            "286A23C899230B93": 19.437,
            "28F6B56C99230BD3": 21.437,
            "28AE6CB399230B6A": 20.562
        }
    }
    """

    measurement = content['measurement']
    tags = {'delivery_method':'http'} | content.get('tags', {})
    arduino_millis = content['arduino_millis']
    sensors = content['values']

    write_points = []
    for sensor, value in sensors.items():
        write_points.append({
            "measurement": measurement,
            "tags": tags | {'sensor': sensor},
            "fields": {
                "arduino_millis": arduino_millis,
                "temperature": value
            }
        })


    client.switch_database('room_log')
    client.write_points(write_points)
    print(write_points)
    return 'Success'

@app.route('/fw/update', methods=['GET', 'POST'])
def process_firmware_update():
    content = request.json
    print('Firmware update check:', content)

    # Extract chip ID and version
    chip_id = content.get('ID', 'unknown')
    version = content.get('ver', 'unknown')

    print(f'Chip ID: {chip_id}, Version: {version}')

    # TODO: Implement actual firmware version checking logic
    # For now, always return false (no update available)
    update_available = False

    response = 'true' if update_available else 'false'
    print(f'Update available: {update_available}')

    return response

@app.route('/boot', methods=['GET', 'POST'])
def process_boot():
    content = request.json
    print('Boot data received:', content)

    # Handle nested payload format (only if content is a dict)
    if isinstance(content, dict) and content.get('decoded', {}).get('payload'):
        content = content['decoded']['payload']

    # Check if this is a multi-point format (array of data points)
    if isinstance(content, list):
        # Multi-point format: array of objects
        json_body = [process_log_point(point) for point in content]
    else:
        # Single-point format: single object
        json_body = [process_log_point(content)]

    print('Processed boot data:', json_body)

    # Boot data goes to room_log database (same as other ESP data)
    client.switch_database('room_log')
    client.write_points(json_body)

    return 'Success'

@app.route('/log', methods=['GET', 'POST'])
def process_url():
    content = request.json
    # Handle nested payload format (only if content is a dict)
    if isinstance(content, dict) and content.get('decoded', {}).get('payload'):
        content = content['decoded']['payload']

    # Check if this is a multi-point format (array of data points)
    if isinstance(content, list):
        # Multi-point format: array of objects
        json_body = [process_log_point(point) for point in content]
    else:
        # Single-point format: single object
        json_body = [process_log_point(content)]

    print('-'*30)
    print(request.data)

    print(json_body)
    client.switch_database('room_log')
    client.write_points(json_body)
    print(json_body)
    return 'Success'

if __name__ == '__main__':
    app.run(host='0.0.0.0')