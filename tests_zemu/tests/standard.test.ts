/** ******************************************************************************
 *  (c) 2018 - 2023 Zondax AG
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

import Zemu, { ButtonKind, isTouchDevice, zondaxMainmenuNavigation } from '@zondax/zemu'
import { FlareApp } from '@zondax/ledger-flare'
import { defaultOptions, models, hdpath } from './common'

jest.setTimeout(60000)

describe('Standard', function () {
  test.concurrent.each(models)('can start and stop container', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(models)('main menu', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const nav = zondaxMainmenuNavigation(m.name, [1, 0, 0, 4, -5])
      await sim.navigateAndCompareSnapshots('.', `${m.prefix.toLowerCase()}-mainmenu`, nav.schedule)
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(models)('get app version', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new FlareApp(sim.getTransport())
      const resp = await app.getVersion()

      console.log(resp)

      expect(resp).toHaveProperty('testMode')
      expect(resp).toHaveProperty('major')
      expect(resp).toHaveProperty('minor')
      expect(resp).toHaveProperty('patch')
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(models)('get address', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new FlareApp(sim.getTransport())

      const resp = await app.getAddressAndPubKey(hdpath)

      console.log(resp)

      expect(resp.returnCode).toEqual(0x9000)
      expect(resp.errorMessage).toEqual('No errors')

      const expected_address = 'flare1yalwlqksjyhcf7jsp7mx9tvxc62fwqzdnuen5c'
      const expected_pk = '028b4bfd1f5fbf8575d7b2c9fc7dc0e587ec417b9f29dff89be57759adc8cdd11d'

      expect(resp.compressed_pk!.toString('hex')).toEqual(expected_pk)
      expect(resp.bech32_address).toEqual(expected_address)
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(models)('show address', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({
        ...defaultOptions,
        model: m.name,
        approveKeyword: isTouchDevice(m.name) ? 'Confirm' : '',
        approveAction: ButtonKind.ApproveTapButton,
      })
      const app = new FlareApp(sim.getTransport())

      const respRequest = app.showAddressAndPubKey(hdpath)
      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-show_address`)

      const resp = await respRequest
      console.log(resp)
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(models)('show address - reject', async function (m) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({
        ...defaultOptions,
        model: m.name,
        rejectKeyword: isTouchDevice(m.name) ? 'Confirm' : '',
      })
      const app = new FlareApp(sim.getTransport())

      const respRequest = app.showAddressAndPubKey(hdpath)

      expect(respRequest).rejects.toMatchObject({
        returnCode: 0x6986,
        errorMessage: 'Transaction rejected',
      })

      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndReject('.', `${m.prefix.toLowerCase()}-show_address_reject`)
    } finally {
      await sim.close()
    }
  })
})
