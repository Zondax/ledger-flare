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

import { DeviceManagementKitBuilder, DeviceModelId, type DeviceManagementKit } from '@ledgerhq/device-management-kit'
import { speculosTransportFactory } from '@ledgerhq/device-transport-kit-speculos'
import { FlareApp } from '@zondax/ledger-flare'
import { DMKTransport } from '@zondax/ledger-js'
import Zemu from '@zondax/zemu'

import { ETH_PATH, defaultOptions, hdpath, models } from './common'

jest.setTimeout(180000)

/**
 * Ledger deprecated `@ledgerhq/hw-transport` ahead of the September 2026 DMK cutoff, and
 * `@zondax/ledger-js` now ships a `DMKTransport` that speaks to a device through
 * `dmk.sendApdu()` instead. Unit tests in that repo prove the adapter against a fake DMK;
 * they cannot prove it against a real app.
 *
 * This does. Zemu already runs Speculos, and Speculos already serves the HTTP API that the
 * DMK's own Speculos transport talks to -- `POST /apdu` on the port zemu exposes as
 * `speculosApiPort`. So both transports can be pointed at one emulator running one ELF, and
 * the same operations run through each and compared.
 *
 * Anything that differs is an adapter bug. Nothing else varies.
 */

// The transport is model-independent -- it just moves bytes -- so one model is enough, and
// running all five would multiply CI time without proving anything further.
const NANOSP = models.find(m => m.name === 'nanosp')!

// Multi-chunk payload: BaseApp splits anything over CHUNK_SIZE across several APDUs, so this
// is what exercises the INIT/ADD/LAST payload-type handling rather than a single exchange.
const CHUNKED_BLOB = Buffer.from(
  '0000000000010000007278db5c30bed04c05ce209179812850bbb3fe6d46d7eef3744d814c0da55524790000000000000000000000000000000000000000000000000000000000000000000000015a6a8c28a2fc040df3b7490440c50f00099c957a000000028fb5f04058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd000000000000001c0000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000700000002541b264000000000000000000000000100000001db89a2339639a5f3fa183258cfea265e4d1cce6c',
  'hex'
)

/**
 * Builds a Device Management Kit bound to the Speculos instance this Zemu already started,
 * and opens a session on it.
 */
async function connectDmk(sim: Zemu): Promise<{ dmk: DeviceManagementKit; sessionId: string }> {
  const speculosUrl = `http://127.0.0.1:${sim.speculosApiPort}`

  const dmk = new DeviceManagementKitBuilder()
    .addTransport(speculosTransportFactory(speculosUrl, false, DeviceModelId.NANO_SP))
    .build()

  // startDiscovering yields an Observable. Subscribing directly avoids taking on rxjs as a
  // direct dependency of the test suite just to call firstValueFrom.
  const sessionId = await new Promise<string>((resolve, reject) => {
    const sub = dmk.startDiscovering({}).subscribe({
      next: (device: Parameters<typeof dmk.connect>[0]['device']) => {
        dmk
          .connect({ device })
          .then((id: string) => {
            sub.unsubscribe()
            resolve(id)
          })
          .catch(reject)
      },
      error: reject,
    })
  })

  return { dmk, sessionId }
}

/**
 * Walks the review screens and approves.
 *
 * Snapshots are written (zemu drives its own screen-change detection from them, so
 * disabling them stalls navigation) but never compared against a golden set: this test
 * asserts on the bytes the device returns, not on what it drew, so there is no snapshot
 * set here to maintain.
 */
async function approve(sim: Zemu, testcaseName: string): Promise<void> {
  await sim.navigateUntilText('.', testcaseName, sim.startOptions.approveKeyword, true, true)
}

describe('DMK transport', function () {
  test('getVersion matches across both transports', async function () {
    const sim = new Zemu(NANOSP.path)
    try {
      await sim.start({ ...defaultOptions, model: NANOSP.name })

      const viaLegacy = await new FlareApp(sim.getTransport()).getVersion()

      const { dmk, sessionId } = await connectDmk(sim)
      const viaDmk = await new FlareApp(new DMKTransport(dmk, sessionId)).getVersion()

      expect(viaDmk).toEqual(viaLegacy)
    } finally {
      await sim.close()
    }
  })

  test('getAddressAndPubKey matches across both transports', async function () {
    const sim = new Zemu(NANOSP.path)
    try {
      await sim.start({ ...defaultOptions, model: NANOSP.name })

      const viaLegacy = await new FlareApp(sim.getTransport()).getAddressAndPubKey(hdpath)

      const { dmk, sessionId } = await connectDmk(sim)
      const viaDmk = await new FlareApp(new DMKTransport(dmk, sessionId)).getAddressAndPubKey(hdpath)

      // Real key material, not just a status word -- a mangled APDU would not land here.
      expect(viaDmk.compressed_pk.toString('hex')).toEqual(viaLegacy.compressed_pk.toString('hex'))
      expect(viaDmk.bech32_address).toEqual(viaLegacy.bech32_address)
    } finally {
      await sim.close()
    }
  })

  test('getEVMAddress matches, so the hw-app-eth path survives on a DMK session', async function () {
    const sim = new Zemu(NANOSP.path)
    try {
      await sim.start({ ...defaultOptions, model: NANOSP.name })

      const viaLegacy = await new FlareApp(sim.getTransport()).getEVMAddress(ETH_PATH, false)

      // FlareApp builds an `Eth` in its constructor, and hw-app-eth calls
      // transport.decorateAppAPIMethods from its own. Reaching this line at all is the
      // proof that DMKTransport's legacy no-op hooks are doing their job.
      const { dmk, sessionId } = await connectDmk(sim)
      const viaDmk = await new FlareApp(new DMKTransport(dmk, sessionId)).getEVMAddress(ETH_PATH, false)

      expect(viaDmk).toEqual(viaLegacy)
    } finally {
      await sim.close()
    }
  })

  /**
   * Signing needs the review screens walked and approved, and that cannot share one emulator
   * with a live DMK session: the DMK's Speculos transport polls `/events` to detect
   * disconnects, which competes with the same stream zemu drives its screen-change detection
   * from. With a session open, zemu stops seeing the screen change and navigation stalls --
   * the non-interactive cases above are unaffected because they never navigate.
   *
   * So each transport gets its own emulator. The comparison still holds: same ELF, same seed,
   * same path, same blob, and Ledger signing is deterministic, so a matching signature is
   * exactly as strong a result as it would be on one instance.
   */
  async function signWith(useDmk: boolean, testcaseName: string) {
    const sim = new Zemu(NANOSP.path)
    try {
      await sim.start({ ...defaultOptions, model: NANOSP.name })

      let app: FlareApp
      if (useDmk) {
        const { dmk, sessionId } = await connectDmk(sim)
        app = new FlareApp(new DMKTransport(dmk, sessionId))
      } else {
        app = new FlareApp(sim.getTransport())
      }

      const request = app.sign(hdpath, CHUNKED_BLOB)
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await approve(sim, testcaseName)
      return await request
    } finally {
      await sim.close()
    }
  }

  test('a chunked signature matches across both transports', async function () {
    const viaLegacy = await signWith(false, 'dmk-sign-legacy')
    const viaDmk = await signWith(true, 'dmk-sign-via-dmk')

    expect(viaDmk.r?.toString('hex')).toEqual(viaLegacy.r?.toString('hex'))
    expect(viaDmk.s?.toString('hex')).toEqual(viaLegacy.s?.toString('hex'))
    expect(viaDmk.v?.toString('hex')).toEqual(viaLegacy.v?.toString('hex'))
  })
})
