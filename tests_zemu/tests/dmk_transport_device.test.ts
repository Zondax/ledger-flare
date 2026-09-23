/** ******************************************************************************
 *  (c) 2018 - 2026 Zondax AG
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ******************************************************************************* */

import { DeviceManagementKitBuilder, type DeviceManagementKit } from '@ledgerhq/device-management-kit'
import { nodeHidTransportFactory } from '@ledgerhq/device-transport-kit-node-hid'
import TransportNodeHid from '@ledgerhq/hw-transport-node-hid'
import { FlareApp } from '@zondax/ledger-flare'
import { DMKTransport } from '@zondax/ledger-js'

import { ETH_PATH, hdpath } from './common'

/**
 * Differential test of the Device Management Kit transport against a real device.
 *
 * Ledger deprecated `@ledgerhq/hw-transport` ahead of the September 2026 DMK cutoff, and
 * `@zondax/ledger-js` ships a `DMKTransport` that reaches the device through
 * `dmk.sendApdu()` instead. This runs the same read-only operations over the legacy
 * transport and over the adapter, and asserts the device answers identically. Anything
 * that differs is an adapter bug.
 *
 * Every case is non-interactive -- no button presses -- so the run is unattended.
 *
 * ## Running it
 *
 * ```
 * pnpm test:device
 * ```
 *
 * Requires, and does not check for you:
 *   - a Ledger connected over USB and unlocked
 *   - the **Flare app open on the device** (not the dashboard)
 *   - Ledger Live closed -- it claims the HID interface
 *
 * Skipped unless `LEDGER_HW_TEST=1`, so CI and `make zemu_test` never open a device.
 *
 * ## Why the EVM case matters here
 *
 * FlareApp builds an `Eth` from `@ledgerhq/hw-app-eth` in its own constructor, and
 * hw-app-eth calls `transport.decorateAppAPIMethods` from *its* constructor. Without that
 * hook on `DMKTransport`, merely constructing FlareApp over a DMK session throws before a
 * single byte is sent. Since @zondax/ledger-js 2.0.0 the hook is not a no-op: it wraps each
 * decorated method in an app-API lock, so an overlapping call rejects with
 * `DMKTransportLockedError`, matching hw-transport. `getEVMAddress` returning at all is the
 * proof that hook works on real hardware, not just against Speculos.
 *
 * ## Why each transport opens its own connection
 *
 * A Ledger exposes a single HID interface and it cannot be claimed twice, so the legacy
 * transport is closed before the DMK session opens. Each case reads the device twice in
 * sequence, which is also why this runs with --runInBand.
 */

const HW_ENABLED = process.env.LEDGER_HW_TEST === '1'
const describeDevice = HW_ENABLED ? describe : describe.skip

/**
 * How long to wait for a device before giving up, in ms.
 *
 * Both paths need an explicit bound: `TransportNodeHid.create()` defaults to waiting
 * forever and node-hid keeps the event loop alive while it does, so with no device
 * attached the run hangs in a way jest's own test timeout cannot interrupt.
 */
const DISCOVERY_TIMEOUT_MS = 5_000

async function withLegacy<T>(fn: (app: FlareApp) => Promise<T>): Promise<T> {
  const transport = await TransportNodeHid.create(DISCOVERY_TIMEOUT_MS, DISCOVERY_TIMEOUT_MS)
  try {
    return await fn(new FlareApp(transport))
  } finally {
    await transport.close()
  }
}

async function withDmk<T>(fn: (app: FlareApp) => Promise<T>): Promise<T> {
  const dmk: DeviceManagementKit = new DeviceManagementKitBuilder().addTransport(nodeHidTransportFactory).build()

  const sessionId = await new Promise<string>((resolve, reject) => {
    const timer = setTimeout(() => {
      reject(new Error(`No Ledger device discovered over USB within ${DISCOVERY_TIMEOUT_MS}ms`))
    }, DISCOVERY_TIMEOUT_MS)
    const sub = dmk.startDiscovering({}).subscribe({
      next: (device: Parameters<typeof dmk.connect>[0]['device']) => {
        dmk
          .connect({ device })
          .then((id: string) => {
            clearTimeout(timer)
            sub.unsubscribe()
            resolve(id)
          })
          .catch(reject)
      },
      error: reject,
    })
  })

  try {
    return await fn(new FlareApp(new DMKTransport(dmk, sessionId)))
  } finally {
    await dmk.disconnect({ sessionId })
    dmk.close()
  }
}

jest.setTimeout(120000)

describeDevice('DMK transport on real hardware (Flare)', function () {
  test('getVersion matches across both transports', async function () {
    const viaLegacy = await withLegacy(app => app.getVersion())
    const viaDmk = await withDmk(app => app.getVersion())

    console.log('legacy:', JSON.stringify(viaLegacy))
    console.log('dmk   :', JSON.stringify(viaDmk))
    expect(viaDmk).toEqual(viaLegacy)
  })

  test('getAddressAndPubKey matches across both transports', async function () {
    const viaLegacy = await withLegacy(app => app.getAddressAndPubKey(hdpath))
    const viaDmk = await withDmk(app => app.getAddressAndPubKey(hdpath))

    // Real key material derived on the device, not just a status word.
    console.log('address:', viaLegacy.bech32_address, '->', viaDmk.bech32_address)
    expect(viaDmk.compressed_pk.toString('hex')).toEqual(viaLegacy.compressed_pk.toString('hex'))
    expect(viaDmk.bech32_address).toEqual(viaLegacy.bech32_address)
  })

  test('getEVMAddress matches, so the hw-app-eth path survives on a DMK session', async function () {
    const viaLegacy = await withLegacy(app => app.getEVMAddress(ETH_PATH, false))
    const viaDmk = await withDmk(app => app.getEVMAddress(ETH_PATH, false))

    expect(viaDmk).toEqual(viaLegacy)
  })
})
