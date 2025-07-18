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

import Zemu, { ButtonKind, isTouchDevice, TouchNavigation } from '@zondax/zemu'
import { FlareApp } from '@zondax/ledger-flare'
import { models, hdpath, defaultOptions, ETH_PATH } from './common'
import secp256k1 from 'secp256k1'
import { createHash } from 'crypto'
import { sha256 } from 'js-sha256'
import { ec } from 'elliptic'

const TEST_DATA = [
  {
    name: 'coston_export_c_to_p',
    blob: Buffer.from(
      '0000000000010000007278db5c30bed04c05ce209179812850bbb3fe6d46d7eef3744d814c0da55524790000000000000000000000000000000000000000000000000000000000000000000000015a6a8c28a2fc040df3b7490440c50f00099c957a000000028fb5f04058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd000000000000001c0000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000700000002541b264000000000000000000000000100000001db89a2339639a5f3fa183258cfea265e4d1cce6c',
      'hex',
    ),
  },
  {
    name: 'coston_import_p_from_c',
    blob: Buffer.from(
      '0000000000110000007200000000000000000000000000000000000000000000000000000000000000000000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000700000002540be40000000000000000000000000100000001db89a2339639a5f3fa183258cfea265e4d1cce6c000000000000000078db5c30bed04c05ce209179812850bbb3fe6d46d7eef3744d814c0da55524790000000114303038e53caca8410bed68d5dd0f8e3a397d6e64657d83319133490fb5cd9b0000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000500000002541b26400000000100000000',
      'hex',
    ),
  },
  {
    name: 'add_permissionless_delegator',
    blob: Buffer.from(
      '00000000001a0000007200000000000000000000000000000000000000000000000000000000000000000000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000700000c7bbb20ce00000000000000000000000001000000019198a74bed93e968051bbdbd84a37a0a5c20c09c00000001c7a99bb2da18fd79adc998fa3544d8bf933172cda43092fdd6da470a206cc18c0000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd00000005000039f5435dee00000000010000000000000000664b4924a25af8be5f07052b2c2e582f7c10a65400000000683d5eec00000000689dcfc000002d79883d200000000000000000000000000000000000000000000000000000000000000000000000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000700002d79883d2000000000000000000000000001000000019198a74bed93e968051bbdbd84a37a0a5c20c09c0000000b000000000000000000000001000000019198a74bed93e968051bbdbd84a37a0a5c20c09c',
      'hex',
    ),
  },
  {
    name: 'add_permissionless_validator',
    blob: Buffer.from(
      '0000000000190000007200000000000000000000000000000000000000000000000000000000000000000000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd00000007000011d59a463a80000000000000000000000001000000019198a74bed93e968051bbdbd84a37a0a5c20c09c000000059200c2050884a47b350c9a95860c961d48bd53a36eed9ab1df8d8338630ea25a0000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd00000005000020e7ea185f40000000010000000095bdc4916bd403d4a1d79e82fc8e795d671ef374f3fb5181cb1b1e8246356f750000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000500037619ecaaabc00000000100000000c7a99bb2da18fd79adc998fa3544d8bf933172cda43092fdd6da470a206cc18c0000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd00000005000039f5435dee000000000100000000d82311262b2692d26e31bb69b0504f81316475cec12d9d239ea6a3c3394de1d90000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd00000005000000003b9aca000000000100000000e4060c85511274cffa695e6d7750adb2dd0f71d88fb3dcf708820986cfb6f5bf0000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000500002d79883d200000000001000000000000000014730ff26ca098fa9136be29b6516bd1a26f325a00000000684015600000000068a83fa00003ec9b43b2a88000000000000000000000000000000000000000000000000000000000000000000000001c8bb576f96756931c6b3254185e4120f29424bd6d35cb2f057227fdf4174a816295398429cb0950aed3088af6cb5d2cc7aac7ce093e81ac934e3d7d2849debb9cffaa06e116a2db7f68584096e9f85436f2c4e0ec85b52b71349f5d14bcde4e7d10d39cd9ac16ec74833fd7fcb24d7b093915a4bb0e8d5c7237212a42f5945e3ed4662c8b540fae4bc904b9a8226e844c0000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd000000070003ec9b43b2a880000000000000000000000001000000019198a74bed93e968051bbdbd84a37a0a5c20c09c0000000b000000000000000000000001000000019198a74bed93e968051bbdbd84a37a0a5c20c09c0000000b000000000000000000000001000000019198a74bed93e968051bbdbd84a37a0a5c20c09c000f4240',
      'hex',
    ),
  },
  {
    name: 'coston_export_p_to_c',
    blob: Buffer.from(
      '00000000001200000072000000000000000000000000000000000000000000000000000000000000000000000000000000045117975f97bc264fd8e80b42f65660378b5f9f03721f1ed59e81bcb14ba2ad120000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000500000002541b264000000001000000005117975f97bc264fd8e80b42f65660378b5f9f03721f1ed59e81bcb14ba2ad120000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd00000005000000e8d4a510000000000100000000b993f9ccf0cedd0ee0d62013b052b6cfad7ffd2a6db79df6a022d1e36a9e729e0000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000500002d79883d20000000000100000000f6702563aa01db271a8cdce2230c512df55c75e3226fa6a469354bd6f57886750000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000500038d7ea4c6800000000001000000000000000078db5c30bed04c05ce209179812850bbb3fe6d46d7eef3744d814c0da55524790000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd000000070003bbe355b4940000000000000000000000000100000001db89a2339639a5f3fa183258cfea265e4d1cce6c',
      'hex',
    ),
  },
  {
    name: 'coston_import_c_from_p',
    blob: Buffer.from(
      '0000000000000000007278db5c30bed04c05ce209179812850bbb3fe6d46d7eef3744d814c0da55524790000000000000000000000000000000000000000000000000000000000000000000000016eba2ff0048fed279c0a982faf2e406985f8040e502eb52ed02e4620679bf1db0000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd000000050003bbe355b494000000000100000000000000015a6a8c28a2fc040df3b7490440c50f00099c957a0003bbe355b04b5258734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd',
      'hex',
    ),
  },
  {
    name: 'flare_export_c_to_p',
    blob: Buffer.from(
      '0000000000010000000e77d3074dc510f43b09ac5be77edee276ef3b55f0097d504846aa8eec613fc6250000000000000000000000000000000000000000000000000000000000000000000000015a6a8c28a2fc040df3b7490440c50f00099c957a000000003bae54eeb3462fc39568bf99fd346d3cdcfe1fb900f14cdcf276e3c0d95f814af8537824000000000000000000000001b3462fc39568bf99fd346d3cdcfe1fb900f14cdcf276e3c0d95f814af853782400000007000000003baa0c4000000000000000000000000100000001db89a2339639a5f3fa183258cfea265e4d1cce6c',
      'hex',
    ),
  },
  {
    name: 'flare_import_p_from_c',
    blob: Buffer.from(
      '0000000000110000000e000000000000000000000000000000000000000000000000000000000000000000000001b3462fc39568bf99fd346d3cdcfe1fb900f14cdcf276e3c0d95f814af853782400000007000000007f33c82000000000000000000000000100000001db89a2339639a5f3fa183258cfea265e4d1cce6c000000000000000077d3074dc510f43b09ac5be77edee276ef3b55f0097d504846aa8eec613fc625000000016612826cd00642f13705525ed4f591b9317fd9b40542c373e29d7860b7514d8000000000b3462fc39568bf99fd346d3cdcfe1fb900f14cdcf276e3c0d95f814af853782400000005000000007f430a600000000100000000',
      'hex',
    ),
  },
  {
    name: 'flare_export_p_to_c',
    blob: Buffer.from(
      '0000000000120000000e000000000000000000000000000000000000000000000000000000000000000000000000000000021bcd204382b5e2e4313fa5ccc536bef60f91b5c2384ff8a6906f9d0c540abc2900000000b3462fc39568bf99fd346d3cdcfe1fb900f14cdcf276e3c0d95f814af853782400000005000000007f33c8200000000100000000269adf76e03d3738099873b49e6ad4c7720a5a21c56d62880d881417fe75069f00000000b3462fc39568bf99fd346d3cdcfe1fb900f14cdcf276e3c0d95f814af853782400000005000000007f33c82000000001000000000000000077d3074dc510f43b09ac5be77edee276ef3b55f0097d504846aa8eec613fc62500000001b3462fc39568bf99fd346d3cdcfe1fb900f14cdcf276e3c0d95f814af85378240000000700000000fe584e0000000000000000000000000100000001db89a2339639a5f3fa183258cfea265e4d1cce6c',
      'hex',
    ),
  },
  {
    name: 'flare_import_c_from_p',
    blob: Buffer.from(
      '0000000000000000000e77d3074dc510f43b09ac5be77edee276ef3b55f0097d504846aa8eec613fc62500000000000000000000000000000000000000000000000000000000000000000000000184ad2d30288f0773277e2a175a5e4d92f32e84ae4f81a05761d7bc581a17aaba00000000b3462fc39568bf99fd346d3cdcfe1fb900f14cdcf276e3c0d95f814af85378240000000500000000fe584e000000000100000000000000015a6a8c28a2fc040df3b7490440c50f00099c957a00000000fe53ad96b3462fc39568bf99fd346d3cdcfe1fb900f14cdcf276e3c0d95f814af8537824',
      'hex',
    ),
  },
  {
    name: 'base_tx',
    blob: Buffer.from(
      '0000000000220000007200000000000000000000000000000000000000000000000000000000000000000000000258734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd00000007000000000098968000000000000000000000000100000001e82db275bf45d4a1fc48b1b05df9f758b9f10f4058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd00000007000000003af2f14000000000000000000000000100000001842e184fe5b9b7f87666bc687af517feabfd1da200000001256f3638bedcd15011f738fe5d0cad8b089863bb73144186a0daa61c2897eaf20000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd00000005000000003b9aca00000000010000000000000000',
      'hex',
    ),
  },
]

jest.setTimeout(120000)

describe.each(models)('Transactions', function (m) {
  test.concurrent.each(TEST_DATA)('Sign transaction', async function (data) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new FlareApp(sim.getTransport())

      const responseAddr = await app.getAddressAndPubKey(hdpath)
      expect(responseAddr.returnCode).toEqual(0x9000)

      const pubKeyRaw = new Uint8Array(responseAddr.compressed_pk!)
      const pubKey = secp256k1.publicKeyConvert(pubKeyRaw, true)

      // do not wait here.. we need to navigate
      const signatureRequest = app.sign(hdpath, data.blob)

      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-sign-${data.name}`)

      const signatureResponse = await signatureRequest

      expect(signatureResponse).toHaveProperty('s')
      expect(signatureResponse).toHaveProperty('r')
      expect(signatureResponse).toHaveProperty('v')

      const EC = new ec('secp256k1')
      const signature_obj = {
        r: signatureResponse.r!,
        s: signatureResponse.s!,
      }
      // Now verify the signature
      const message = createHash('sha256').update(data.blob).digest()
      const valid = EC.verify(message, signature_obj, Buffer.from(pubKey), 'hex')
      expect(valid).toEqual(true)
    } finally {
      await sim.close()
    }
  })

  test.concurrent.each(TEST_DATA)('Sign transaction expert', async function (data) {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new FlareApp(sim.getTransport())

      //Change to expert mode so we can skip fields
      await sim.toggleExpertMode()

      const responseAddr = await app.getAddressAndPubKey(hdpath)
      expect(responseAddr.returnCode).toEqual(0x9000)

      const pubKeyRaw = new Uint8Array(responseAddr.compressed_pk!)
      const pubKey = secp256k1.publicKeyConvert(pubKeyRaw, true)

      // do not wait here.. we need to navigate
      const signatureRequest = app.sign(hdpath, data.blob)

      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-sign-${data.name}-expert`)

      const signatureResponse = await signatureRequest

      expect(signatureResponse).toHaveProperty('s')
      expect(signatureResponse).toHaveProperty('r')
      expect(signatureResponse).toHaveProperty('v')
      expect(signatureResponse.returnCode).toEqual(0x9000)
      expect(signatureResponse.errorMessage).toEqual('No errors')

      const EC = new ec('secp256k1')
      const signature_obj = {
        r: signatureResponse.r!,
        s: signatureResponse.s!,
      }
      // Now verify the signature
      const message = createHash('sha256').update(data.blob).digest()
      const valid = EC.verify(message, signature_obj, Buffer.from(pubKey), 'hex')
      expect(valid).toEqual(true)
    } finally {
      await sim.close()
    }
  })

  test.concurrent('sign hash', async function () {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new FlareApp(sim.getTransport())

      const responseAddr = await app.getAddressAndPubKey(hdpath)
      expect(responseAddr.returnCode).toEqual(0x9000)

      const pubKeyRaw = new Uint8Array(responseAddr.compressed_pk!)
      const pubKey = secp256k1.publicKeyConvert(pubKeyRaw, true)

      // Enable blind signing mode (this need to be fixed on zemu, as the current fn is not working anymore)
      await sim.toggleBlindSigning()

      const text = 'FlareApp'
      const msg = Buffer.from(sha256(text), 'hex')
      const signatureRequest = app.signHash(hdpath, msg)

      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-sign-hash`, true, 0, 1500, true)

      const signatureResponse = await signatureRequest

      expect(signatureResponse).toHaveProperty('s')
      expect(signatureResponse).toHaveProperty('r')
      expect(signatureResponse).toHaveProperty('v')

      const EC = new ec('secp256k1')
      const signature_obj = {
        r: signatureResponse.r!,
        s: signatureResponse.s!,
      }
      // Now verify the signature
      const valid = EC.verify(msg, signature_obj, Buffer.from(pubKey), 'hex')
      expect(valid).toEqual(true)
    } finally {
      await sim.close()
    }
  })

  test.concurrent('sign tx with 44/60', async function () {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new FlareApp(sim.getTransport())

      const responseAddr = await app.getAddressAndPubKey(ETH_PATH)
      expect(responseAddr.returnCode).toEqual(0x9000)

      const pubKeyRaw = new Uint8Array(responseAddr.compressed_pk!)
      const pubKey = secp256k1.publicKeyConvert(pubKeyRaw, true)

      const signatureRequest = app.sign(ETH_PATH, TEST_DATA[0].blob)

      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-sign-tx-eth-path`)

      const signatureResponse = await signatureRequest

      expect(signatureResponse).toHaveProperty('s')
      expect(signatureResponse).toHaveProperty('r')
      expect(signatureResponse).toHaveProperty('v')

      const EC = new ec('secp256k1')
      const signature_obj = {
        r: signatureResponse.r!,
        s: signatureResponse.s!,
      }
      // Now verify the signature
      const message = createHash('sha256').update(TEST_DATA[0].blob).digest()
      const valid = EC.verify(message, signature_obj, Buffer.from(pubKey), 'hex')
      expect(valid).toEqual(true)
    } finally {
      await sim.close()
    }
  })

  test.concurrent('Multiple Signatures', async function () {
    const sim = new Zemu(m.path)
    try {
      await sim.start({ ...defaultOptions, model: m.name })
      const app = new FlareApp(sim.getTransport())

      const responseAddr = await app.getAddressAndPubKey(hdpath)
      expect(responseAddr.returnCode).toEqual(0x9000)

      const pubKeyRaw = new Uint8Array(responseAddr.compressed_pk!)
      const pubKey = secp256k1.publicKeyConvert(pubKeyRaw, true)

       const blob1 = Buffer.from(
      '0000000000010000007278db5c30bed04c05ce209179812850bbb3fe6d46d7eef3744d814c0da55524790000000000000000000000000000000000000000000000000000000000000000000000015a6a8c28a2fc040df3b7490440c50f00099c957a000000028fb5f04058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd000000000000001c0000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000700000002541b264000000000000000000000000100000001db89a2339639a5f3fa183258cfea265e4d1cce6c',
      'hex')

      // do not wait here.. we need to navigate
      const signatureRequest1 = app.sign(hdpath, blob1)

      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      const lastSnapshotIdx = await sim.navigateUntilText(
        '.',
        `${m.prefix.toLowerCase()}-multiple_signatures`,
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
        await sim.takeSnapshotAndOverwrite('.', `${m.prefix.toLowerCase()}-multiple_signatures`, lastSnapshotIdx);
      }

      sim.compareSnapshots('.', `${m.prefix.toLowerCase()}-multiple_signatures`, lastSnapshotIdx);

      const signatureResponse1 = await signatureRequest1

      expect(signatureResponse1).toHaveProperty('s')
      expect(signatureResponse1).toHaveProperty('r')
      expect(signatureResponse1).toHaveProperty('v')

      let EC = new ec('secp256k1')
      const signature_obj1 = {
        r: signatureResponse1.r!,
        s: signatureResponse1.s!,
      }
      // Now verify the signature
      const message1 = createHash('sha256').update(blob1).digest()
      const valid1 = EC.verify(message1, signature_obj1, Buffer.from(pubKey), 'hex')
      expect(valid1).toEqual(true)

      // deleteEvents to start second signature
      await sim.deleteEvents()

       const blob2 = Buffer.from(
      '0000000000110000007200000000000000000000000000000000000000000000000000000000000000000000000158734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000700000002540be40000000000000000000000000100000001db89a2339639a5f3fa183258cfea265e4d1cce6c000000000000000078db5c30bed04c05ce209179812850bbb3fe6d46d7eef3744d814c0da55524790000000114303038e53caca8410bed68d5dd0f8e3a397d6e64657d83319133490fb5cd9b0000000058734f94af871c3d131b56131b6fb7a0291eacadd261e69dfb42a9cdf6f7fddd0000000500000002541b26400000000100000000',
      'hex')

      const signatureRequest2 = app.sign(hdpath, blob2)

      // Wait until we are not in the main menu
      await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot())
      await sim.compareSnapshotsAndApprove('.', `${m.prefix.toLowerCase()}-multiple_signatures`, true, lastSnapshotIdx + 1)

      const signatureResponse2 = await signatureRequest2

      expect(signatureResponse2).toHaveProperty('s')
      expect(signatureResponse2).toHaveProperty('r')
      expect(signatureResponse2).toHaveProperty('v')

      EC = new ec('secp256k1')
      const signature_obj2 = {
        r: signatureResponse2.r!,
        s: signatureResponse2.s!,
      }
      // Now verify the signature
      const message2 = createHash('sha256').update(blob2).digest()
      const valid2 = EC.verify(message2, signature_obj2, Buffer.from(pubKey), 'hex')
      expect(valid2).toEqual(true)
    } finally {
      await sim.close()
    }
  })
})
