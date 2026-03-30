import {Zcl} from 'zigbee-herdsman';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';
import {presets as e, access as ea} from 'zigbee-herdsman-converters/lib/exposes';
import * as utils from 'zigbee-herdsman-converters/lib/utils';

function minMax() {
    const exposes = [];
    exposes.push(
        e.enum("setLimits", ea.SET, ["setMin", "setMax"]).withDescription("Set device limits").withCategory("config")
    );

    const toZigbee = [
        {
            key: ["setLimits"],
            convertSet: async (entity, key, value, meta) => {
                utils.assertEndpoint(entity);

                await entity.command("tcSpecificBlind", value, {});

                return {};
            }
        }
    ];

    return {exposes, toZigbee, isModernExtend: true};
}

function blindNudge() {
    const exposes = [];
    exposes.push(
        e.composite("nudge", "nudge", ea.SET)
            .withDescription("Nudge position")
            .withFeature(e.numeric("distance", ea.SET))
    );

    const toZigbee = [
        {
            key: ["nudge"],
            convertSet: async (entity, key, value, meta) => {
                utils.assertEndpoint(entity);

                const data = { distance: value.distance };
                await entity.command("tcSpecificBlind", "nudge", data);

                return {};
            }
        }
    ];

    return {exposes, toZigbee, isModernExtend: true};
}

export default {
    zigbeeModel: ['Blinds'],
    model: 'Blinds',
    vendor: 'TC',
    icon: 'device_icons/blind.png',
    description: 'Somfy zigbee retrofit',
    extend: [
        m.deviceAddCustomCluster("tcSpecificBlind", {
            manufacturerCode: 0x1234,
            ID: 0xFC13,
            attributes: {
                'setup': { ID: 0x0001, type: Zcl.DataType.BOOLEAN, write: true },
                'minSpeed': { ID: 0x0002, type: Zcl.DataType.INT32, write: true, max: 0xffff },
                'invert': { ID: 0x0003, type: Zcl.DataType.BOOLEAN, write: true }
            },
            commands: {
                setMin: { ID: 0xF1, parameters: [] },
                setMax: { ID: 0xF2, parameters: [] },
                nudge: { ID: 0xF3, parameters: [
                    { name: "distance", type: Zcl.DataType.INT16 }
                ] }
            },
            commandsResponse: {}
        }),
        m.windowCovering({
            controls: ["lift"]
        }),
        m.numeric({
            name: "velocityLift",
            label: "Velocity",
            description: "Lift velocity",
            cluster: "closuresWindowCovering",
            attribute: "velocityLift",
            valueMin: 0,
            valueMax: 65534
        }),
        m.numeric({
            name: "minSpeed",
            label: "Min Speed",
            description: "Minimum speed when nearing end of movement",
            cluster: "tcSpecificBlind",
            attribute: "minSpeed",
            valueMin: 0,
            valueMax: 65534
        }),
        m.binary({
            name: 'setup',
            valueOn: ['ON', 1],
            valueOff: ['OFF', 0],
            cluster: 'tcSpecificBlind',
            attribute: 'setup',
            description: 'Enable setup mode'
        }),
        m.binary({
            name: 'invert',
            valueOn: ['ON', 1],
            valueOff: ['OFF', 0],
            cluster: 'tcSpecificBlind',
            attribute: 'invert',
            description: 'Invert direction'
        }),
        blindNudge(),
        minMax(),
        m.identify(),
        m.battery({
            voltage: true,
            voltageReporting: true,
            percentageReportingConfig: {min: "1_HOUR", max: "4_HOURS", change: 5},
            voltageReportingConfig: {min: "1_HOUR", max: "4_HOURS", change: 5}
        }),
        m.temperature(),
        m.humidity()
    ],
    ota: true
};
