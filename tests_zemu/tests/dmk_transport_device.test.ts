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
 * The same differential as `dmk_transport.test.ts`, but over real USB instead of Speculos.
 *
 * The emulator suite proves the adapter frames APDUs correctly against a real app. It does
 * not exercise the USB stack: HID framing, the 64-byte report chunking underneath the APDU,
 * device enumeration, or app-open state. Those only appear on hardware, so this is the last
 * gap between "the adapter is correct" and "the adapter works on a device someone owns".
 *
 * ## Running it
 *
 * ```
 * pnpm test:device
 * ```
 *
 * Requires, and does not check for you:
 *   - a Ledger device connected over USB and unlocked
 *   - the Flare app open on it
 *   - no other process holding the device (close Ledger Live first -- it claims the HID
 *     interface and this will fail with an opaque error if it is running)
 *
 * It is skipped unless `LEDGER_HW_TEST=1`, so CI and a normal `pnpm test` never try to open
 * a device. `test:device` sets that for you.
 *
 * ## Why each transport gets its own connection
 *
 * A Ledger exposes a single HID interface and it cannot be claimed twice, so the legacy
 * transport is closed before the DMK session opens. Every case below therefore reads the
 * device twice in sequence rather than holding both at once. That is also why this file
 * runs with --runInBand.
 */

const HW_ENABLED = process.env.LEDGER_HW_TEST === '1'

// Skipping rather than failing: a machine with no device attached is the normal case, and
// this suite is only meaningful when someone has deliberately plugged one in.
const describeDevice = HW_ENABLED ? describe : describe.skip

/**
 * How long to wait for a device before giving up, in ms.
 *
 * Both transports need an explicit bound. `TransportNodeHid.create()` defaults to waiting
 * forever, and node-hid keeps the event loop alive while it does, so with no device attached
 * the run hangs in a way jest's own test timeout cannot interrupt -- the process never exits.
 */
const DISCOVERY_TIMEOUT_MS = 5_000

/** Opens the legacy hw-transport, runs `fn`, and always closes the device again. */
async function withLegacy<T>(fn: (app: FlareApp) => Promise<T>): Promise<T> {
  const transport = await TransportNodeHid.create(DISCOVERY_TIMEOUT_MS, DISCOVERY_TIMEOUT_MS)
  try {
    return await fn(new FlareApp(transport))
  } finally {
    await transport.close()
  }
}

/** Opens a DMK session over node-hid, runs `fn`, and always disconnects again. */
async function withDmk<T>(fn: (app: FlareApp) => Promise<T>): Promise<T> {
  const dmk: DeviceManagementKit = new DeviceManagementKitBuilder().addTransport(nodeHidTransportFactory).build()

  const sessionId = await new Promise<string>((resolve, reject) => {
    const timer = setTimeout(() => {
      sub.unsubscribe()
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

describeDevice('DMK transport on real hardware', function () {
  test('getVersion matches across both transports', async function () {
    const viaLegacy = await withLegacy(app => app.getVersion())
    const viaDmk = await withDmk(app => app.getVersion())

    expect(viaDmk).toEqual(viaLegacy)
  })

  test('getAddressAndPubKey matches across both transports', async function () {
    const viaLegacy = await withLegacy(app => app.getAddressAndPubKey(hdpath))
    const viaDmk = await withDmk(app => app.getAddressAndPubKey(hdpath))

    // Real key material derived on the device, not just a status word.
    expect(viaDmk.compressed_pk.toString('hex')).toEqual(viaLegacy.compressed_pk.toString('hex'))
    expect(viaDmk.bech32_address).toEqual(viaLegacy.bech32_address)
  })

  test('getEVMAddress matches, so the hw-app-eth path survives on a DMK session', async function () {
    // FlareApp builds an `Eth` in its constructor and hw-app-eth calls
    // transport.decorateAppAPIMethods from its own, so reaching the assertion at all
    // exercises DMKTransport's legacy no-op hooks against a real device.
    const viaLegacy = await withLegacy(app => app.getEVMAddress(ETH_PATH, false))
    const viaDmk = await withDmk(app => app.getEVMAddress(ETH_PATH, false))

    expect(viaDmk).toEqual(viaLegacy)
  })

  test('a chunked signature matches across both transports', async function () {
    // Multi-chunk: BaseApp splits anything over CHUNK_SIZE across several APDUs, which is
    // what puts real HID report framing under test rather than a single exchange.
    const blob = Buffer.from(
      '0000000000010000007278db5c30bed04c05ce209179812850bbb3fe6d46d7eef3744d814c0da55524790000000000000000000000000000000000000000000000000000000000000000000000015a6a8c28a2fc040df3b7490440c50f00099c957a000000028fb5f04058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd000000000000001c0000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000700000002541b264000000000000000000000000100000001db89a2339639a5f3fa183258cfea265e4d1cce6c',
      'hex'
    )

    console.log('\n>>> Approve the transaction on the device (1 of 2: legacy transport)\n')
    const viaLegacy = await withLegacy(app => app.sign(hdpath, blob))

    console.log('\n>>> Approve the same transaction again (2 of 2: DMK transport)\n')
    const viaDmk = await withDmk(app => app.sign(hdpath, blob))

    // Ledger signing is deterministic, so the same seed, path and message must produce
    // byte-identical signatures across the two transports.
    expect(viaDmk.r?.toString('hex')).toEqual(viaLegacy.r?.toString('hex'))
    expect(viaDmk.s?.toString('hex')).toEqual(viaLegacy.s?.toString('hex'))
    expect(viaDmk.v?.toString('hex')).toEqual(viaLegacy.v?.toString('hex'))
  })
})
