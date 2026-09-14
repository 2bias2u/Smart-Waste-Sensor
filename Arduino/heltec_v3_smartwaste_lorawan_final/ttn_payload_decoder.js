// TTN Uplink Payload Formatter
// Passend zu heltec_v3_smartwaste_lorawan_final.ino
// Payload: 6 Bytes, Big-Endian
//   Byte 0-1: Distanz 1 (cm), uint16
//   Byte 2-3: Distanz 2 (cm), uint16
//   Byte 4-5: Minimum (cm), uint16

function decodeUplink(input) {
  var bytes = input.bytes;

  if (bytes.length !== 6) {
    return {
      data: {},
      errors: ["Invalid payload length: expected 6 bytes, got " + bytes.length]
    };
  }

  var distance1 = (bytes[0] << 8) | bytes[1];
  var distance2 = (bytes[2] << 8) | bytes[3];
  var minDistance = (bytes[4] << 8) | bytes[5];

  return {
    data: {
      distance1_cm: distance1,
      distance2_cm: distance2,
      minDistance_cm: minDistance
    },
    warnings: [],
    errors: []
  };
}
