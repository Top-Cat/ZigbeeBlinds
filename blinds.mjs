import {Zcl} from 'zigbee-herdsman';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

export default {
    zigbeeModel: ['Blinds'],
    model: 'Blinds',
    vendor: 'TC',
    description: 'Somfy zigbee retrofit',
    extend: [
        m.windowCovering({
            controls: ["lift"],
            coverMode: true
        }),
        m.identify(),
        m.battery({
            voltage: true
        }),
        m.temperature(),
        m.humidity()
    ],
    ota: true
};
