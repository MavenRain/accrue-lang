### G1

A retainer. `egGood` opens the engagement with the client signature, and
`csGood` adds the firm countersignature at 20. `csFundCtl` funds 200 at
30, countersigns at 40 and reads the recognized value at 50. It is a kit
file (examples/skill/kit.txt), so each command uses `--kit`.

File: examples/skill/g1-retainer.lang

```lang
-- PLAN row 529 (G1): a retainer. egGood opens the engagement with the client
-- signature and csGood adds the firm countersignature at 20. csFundCtl funds
-- 200 at 30, countersigns at 40 and reads the recognized value at 50.
def rB : Rate := pair (fmEta Segment (segmentOf 0 50 2u 1u)) (fmEta Mass (massOf 0 40u))

def dB : Dispute := fmMul U256 (fmEta U256 2500u) (fmEta U256 5000u)

def histOf : Instrument -> History :=
  fun (i : Instrument) => fmEta (Prod Time Policy) (pair 0 (policyOf rB (windowOf 0 100) (grantOf i flagNo) dB))

def hPush : History := histOf instrPush

def sAt : Time -> Stamp := fun (t : Time) => stampOf t 0u 0u 0u

def Step : Type 0 := Option (Prod Escrow Payout)

def bkRec : History -> Time -> Escrow -> Option U256 :=
  fun (h : History) (t : Time) (x : Escrow) => elim (fun (u : Escrow) => Option U256) (fun (e : Engagement) (b : Book) => elim (fun (u : Book) => Option U256) (fun (l : Funding) (w : U256) (c : Option Time) => recognizedAt h e l t) b) x

def cK : Point := 0xf2bb893b2497c632d4b2e1bcc9f3565efe400130e944ea3a4ba689a46220baa2u

def fK : Point := 0xfa043432a49afe1c8e4ad857ee8935e7cae984e9f655fbd12117d48849d5727du

def engR : U256 := 0xae00a0168cdcf5811726c150336492d5be4ebb5857965ddb7acde38ffca7a33cu

def engZ : U256 := 0x2c8cc28029354ca5e4f7d87c6807bd594a4bb1ce4de3fb875bbfcf9dbb033828u

def ctrR : U256 := 0x4c496fbafa6f2c546472f4b21bceb13b71945c5b6e6339783a0db63be6b38cfu

def ctrZ : U256 := 0x64e219dee5e8683ee2fb3f4ff4bb43f5a29b34a0393e883c6b88327e5f1c55cbu

def eS : Engagement := engagementOf cK fK 3u 4u (windowOf 10 60) none none

def xS : Escrow := escrowOf eS (bookOf (fmUnit (Prod Time U256)) 0u none)

def egGood : Option Escrow := engage cK fK 3u 4u (windowOf 10 60) (authOf engR engZ)

def csGood : Option Escrow := countersign (sAt 20) (authOf ctrR ctrZ) xS

def csLateFund : Step := escFund (sAt 30) 200u xS

def csFundAt : Time -> Option U256 :=
  fun (t : Time) => bind csLateFund (fun (p : Prod Escrow Payout) => bind (countersign (sAt t) (authOf ctrR ctrZ) (first p)) (bkRec hPush 50))

def csFundCtl : Option U256 := csFundAt 40
```

`build/langc --kit check examples/skill/g1-retainer.lang` gives:

The exit code is 0.

`build/langc --kit eval examples/skill/g1-retainer.lang egGood` gives:

```text
some (escrowOf (engagementOf 109791056892004066315013292490115010174505158101236297434859555777157806471842u 113085639790218053198109643608074705563682068750669178417462628566113730261629u 3u 4u (windowOf 10 60) none none) (bookOf (fmUnit (Prod Nat U256)) 0u none))
```

The exit code is 0.

`build/langc --kit eval examples/skill/g1-retainer.lang csGood` gives:

```text
some (escrowOf (engagementOf 109791056892004066315013292490115010174505158101236297434859555777157806471842u 113085639790218053198109643608074705563682068750669178417462628566113730261629u 3u 4u (windowOf 10 60) (some 20) none) (bookOf (fmUnit (Prod Nat U256)) 0u none))
```

The exit code is 0.

`build/langc --kit eval examples/skill/g1-retainer.lang csFundCtl` gives:

```text
20u
```

The exit code is 0.
