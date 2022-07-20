#ESP32 Pin Settings
device_config = {
    'miso':19,
    'mosi':23,
    'ss':5,
    'sck':18,
    'dio_0':2,
    'reset':14,
    'led':12, 
    'battery':33
}

app_config = {
    'loop': 200,
    'sleep': 100,
}

lora_parameters = {
    'frequency': 915E6, 
    'tx_power_level': 20, 
    'signal_bandwidth': 250E3,    
    'spreading_factor': 10, 
    'coding_rate': 8, 
    'preamble_length': 8,
    'implicit_header': False, 
    'sync_word': 0xE3, 
    'enable_CRC': False,
    'invert_IQ': False,
}

wifi_config = {
    'ssid':'<Your SSID>',
    'password':'<router password>'
}

