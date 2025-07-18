/** ******************************************************************************
 *  (c) 2018 - 2024 Zondax AG
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
import Zemu, { ButtonKind, isTouchDevice } from '@zondax/zemu'
import { FlareApp } from '@zondax/ledger-flare'
import { ETH_PATH, EXPECTED_ETH_ADDRESS, EXPECTED_ETH_PK, defaultOptions, models } from './common'
import { ec } from 'elliptic'
jest.setTimeout(90000)
const sha3 = require('js-sha3')
const SIGN_TEST_DATA = [
  {
    name: 'personal_sign_msg',
    message: Buffer.from('Hello World!', 'utf8'),
    blind: false,
  },
  {
    name: 'personal_sign_big_msg',
    message: Buffer.from('Just a big dummy message to be sign. To test if ew are parsing the chunks in the right way. By: Zondax', 'utf8'),
    blind: false,
  },
  {
    name: 'personal_sign_non_printable_msg',
    message: Buffer.from('\x00\x00\x00\x00\x00zx', 'utf8'),
    blind: true,
  },
]
describe.each(models)('EIP191', function (m) {
  test.concurrent.each(SIGN_TEST_DATA)('sign transaction:  $name', async function (data) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new FlareApp(sim.getTransport())
      const msgData = data.message

      if (data.blind) {
        await sim.toggleBlindSigning()
      }
      // do not wait here..
      const signatureRequest = app.signPersonalMessage(ETH_PATH, msgData.toString('hex'))
      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-eth-${data.name}`, true, 0, 1500, data.blind)
      let resp = await signatureRequest
      console.log(resp)
      const header = Buffer.from('\x19Ethereum Signed Message:\n', 'utf8')
      const msgLengthString = String(msgData.length)
      const msg = Buffer.concat([header, Buffer.from(msgLengthString, 'utf8'), msgData])
      const msgHash = sha3.keccak256(msg)
      const signature_obj = {
        r: Buffer.from(resp.r, 'hex'),
        s: Buffer.from(resp.s, 'hex'),
      }
      // Verify signature
      const EC = new ec('secp256k1')
      const signatureOK = EC.verify(msgHash, signature_obj, Buffer.from(EXPECTED_ETH_PK, 'hex'), 'hex')
      expect(signatureOK).toEqual(true)
    } finally {
      await sim.close()
    }
  })

  test.concurrent('Sign multiple personal messages', async function () {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new FlareApp(sim.getTransport())
      const msgData1 = Buffer.from('Just a big dummy message to be sign. To test if ew are parsing the chunks in the right way. By: Zondax', 'utf8')
      const header = Buffer.from('\x19Ethereum Signed Message:\n', 'utf8')

      // do not wait here..
      const signatureRequest1 = app.signPersonalMessage(ETH_PATH, msgData1.toString('hex'))
        //
      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      const lastSnapshotIdx = await sim.navigateUntilText(
        '.',
        `${m.prefix.toLowerCase()}-multiple_signatures_personal_msg`,
        sim.startOptions.approveKeyword,
        true,
        true,
        0,
        15000,
        true,
        true,
        false
      );

      if (isTouchDevice(sim.startOptions.model)) {
        // Avoid taking a snapshot of the final animation
        await sim.waitUntilScreenIs(sim.mainMenuSnapshot);
        await sim.takeSnapshotAndOverwrite('.', `${m.prefix.toLowerCase()}-multiple_signatures_personal_msg`, lastSnapshotIdx);
      }

      sim.compareSnapshots('.', `${m.prefix.toLowerCase()}-multiple_signatures_personal_msg`, lastSnapshotIdx);

      const resp1 = await signatureRequest1
      const msg1LengthString = String(msgData1.length)
      const msg1 = Buffer.concat([header, Buffer.from(msg1LengthString, 'utf8'), msgData1])
      const msg1Hash = sha3.keccak256(msg1)
      const signature_obj1 = {
        r: Buffer.from(resp1.r, 'hex'),
        s: Buffer.from(resp1.s, 'hex'),
      }
      // Verify signature
      let EC = new ec('secp256k1')
      const signature1OK = EC.verify(msg1Hash, signature_obj1, Buffer.from(EXPECTED_ETH_PK, 'hex'), 'hex')
      expect(signature1OK).toEqual(true)

      // deleteEvents to start second signature
      await sim.deleteEvents()

      const msgData2 = Buffer.from('Hello World!', 'utf8')
      const signatureRequest2 = app.signPersonalMessage(ETH_PATH, msgData2.toString('hex'))
      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-multiple_signatures_personal_msg`, true, lastSnapshotIdx + 1)
      let resp2 = await signatureRequest2
      console.log(resp2)
      const msg2LengthString = String(msgData2.length)
      const msg2 = Buffer.concat([header, Buffer.from(msg2LengthString, 'utf8'), msgData2])
      const msg2Hash = sha3.keccak256(msg2)
      const signature_obj2 = {
        r: Buffer.from(resp2.r, 'hex'),
        s: Buffer.from(resp2.s, 'hex'),
      }
      // Verify signature
      EC = new ec('secp256k1')
      const signatureOK = EC.verify(msg2Hash, signature_obj2, Buffer.from(EXPECTED_ETH_PK, 'hex'), 'hex')
      expect(signatureOK).toEqual(true)

    } finally {
      await sim.close()
    }
  })
})
