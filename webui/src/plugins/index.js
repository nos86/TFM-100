/**
 * plugins/index.js
 *
 * Automatically included in `./src/main.js`
 */

// Plugins
import vuetify from './vuetify'
import { createWebSerialPlugin } from "@/plugins/webserial";

export function registerPlugins (app) {
  app.use(vuetify)
  app.use(createWebSerialPlugin({
  baudRate: 115200,
  textMode: true,
  lineEnding: "\n\r",
  filters: [{usbVendorId: 0x1b4f}], // TFM-100 USB Vendor ID
}));
}
