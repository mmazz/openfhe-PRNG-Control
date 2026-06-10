//==================================================================================
// BSD 2-Clause License
//
// Copyright (c) 2014-2022, NJIT, Duality Technologies Inc. and other contributors
//
// All rights reserved.
//
// Author TPOC: contact@openfhe.org
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==================================================================================

/*
  Simple examples for CKKS
 */

#define PROFILE

#include "openfhe.h"
#include <assert.h>

using namespace lbcrypto;



Ciphertext<DCRTPoly> StdDevCKKS(
    const CryptoContext<DCRTPoly>& cc,
    const Ciphertext<DCRTPoly>& ct,
    uint32_t batchSize)
{
    // 1. mean(x)
    auto sumCt  = cc->EvalSum(ct, batchSize);
    auto meanCt = cc->EvalMult(sumCt, 1.0 / batchSize);

    // 2. centered = x - mean
    auto centered = cc->EvalSub(ct, meanCt);

    // 3. squared deviation (x - mean)^2
    auto squared = cc->EvalMult(centered, centered);

    // 4. mean of squared deviations = variance
    auto sumSq  = cc->EvalSum(squared, batchSize);
    auto varCt  = cc->EvalMult(sumSq, 1.0 / batchSize);

    // 5. sqrt(var) via approximation
    double lowerBound = 0.0;
    double upperBound = 100.0;  // ajustar según escala CKKS real
    int polyDegree = 10;

    auto stdCt = cc->EvalChebyshevFunction(
        [](double x) -> double { return std::sqrt(x); },
        varCt,
        lowerBound,
        upperBound,
        polyDegree
    );

    return stdCt;
}

bool check(
    const CryptoContext<DCRTPoly>& cc,
    const Ciphertext<DCRTPoly>& ct,
    uint32_t batchSize)
{
    bool isGood = true;
    const auto& dcrtpoly = ct->GetElements()[0];
    const auto& limbs    = dcrtpoly.GetAllElements();
    auto elementParams = cc->GetCryptoParameters()->GetElementParams();
    auto ringDim = elementParams->GetRingDimension();

    //const auto& params = elementParams->GetParams();
    //NativeInteger mod = elementParams->GetModulus().ConvertToInt();


    auto params = cc->GetElementParams(); // o los params del contexto
   ct->GetElements()[0].SetFormat(Format::COEFFICIENT); // Cambia a formato coeficiente para acceder a los coeficientes individuales
    auto vplain = ct->GetElements()[0].CRTInterpolate(); // Interpola a formato coeficiente para acceder a los coeficientes individuales
ct->GetElements()[0].SetFormat(Format::EVALUATION);


 //   auto towerParams = elementParams->GetParams()[0];
    // towerParams es shared_ptr<ILNativeParams>

    // 2. Construir el NativePoly con esos params
  //  NativePoly w(towerParams, Format::EVALUATION, true); // true = init a cero

  //  uint32_t n = towerParams->GetRingDimension();
  //  for (uint32_t i = 0; i < n; i++) {
  //      w[i] = NativeInteger(i);
  //  }

 //   NativePoly wv = w*v[0];
//
 ///   w.SetFormat(Format::COEFFICIENT);
//    v.SetFormat(Format::COEFFICIENT);



    BigInteger sum = 0;
    for (uint32_t i=0; i<ringDim; i++){
        sum+=vplain[i];
    }
    
    //sum = sum.Mod(params[0]->GetModulus());
    uint32_t numLimbs = limbs.size();
    std::vector<NativePoly> limbsCoeff(numLimbs);
    for (uint32_t i = 0; i < numLimbs; i++) {
        limbsCoeff[i] = limbs[i];  // copia del NativePoly
        if (limbsCoeff[i].GetFormat() == Format::EVALUATION) {
            limbsCoeff[i].SetFormat(Format::COEFFICIENT);
        }
    }
    BigInteger sum2 = 0;
    for (uint32_t i=0; i<ringDim; i++){
        sum2+=limbs[0][i];
    }
    //sum2 = sum2.Mod(params[0]->GetModulus());

    std::cout << "sum:  " << sum << std::endl;
    std::cout << "sum2: " << sum2 << std::endl;
    isGood = (sum == sum2);
    for (uint32_t i = 0; i < numLimbs; i++) {
        limbsCoeff[i] = limbs[i];  // copia del NativePoly
        if (limbsCoeff[i].GetFormat() == Format::COEFFICIENT) {
            limbsCoeff[i].SetFormat(Format::EVALUATION);
        }
    }
    return isGood;
}


BigInteger checkSum_limb(
    const Ciphertext<DCRTPoly>& ct,
    const BigInteger& alpha,
    const BigInteger& q_check,
    uint32_t polyIndex = 0)
{
    // Accedemos a los limbs RNS directamente sin tocar el DCRTPoly
    const auto& dcrtpoly = ct->GetElements()[polyIndex];
    const auto& limbs    = dcrtpoly.GetAllElements();  // vector<NativePoly>

    uint32_t numLimbs = limbs.size();

    // Cada NativePoly puede estar en EVALUATION o COEFFICIENT
    // Hacemos copia de cada limb individualmente
    std::vector<NativePoly> limbsCoeff(numLimbs);
    for (uint32_t i = 0; i < numLimbs; i++) {
        limbsCoeff[i] = limbs[i];  // copia del NativePoly
        if (limbsCoeff[i].GetFormat() == Format::EVALUATION) {
            limbsCoeff[i].SetFormat(Format::COEFFICIENT);
        }
    }

    uint32_t ringDim = limbsCoeff[0].GetLength();
    BigInteger s(0);
    BigInteger power(1);

    for (uint32_t j = 0; j < ringDim; j++) {
        BigInteger coeff_j(0);
        for (uint32_t i = 0; i < numLimbs; i++) {
            BigInteger limb_val(limbsCoeff[i][j].ConvertToInt());
            coeff_j = (coeff_j + limb_val).Mod(q_check);
        }
        s     = (s + coeff_j * power).Mod(q_check);
        power = (power * alpha).Mod(q_check);
    }
    for (uint32_t i = 0; i < numLimbs; i++) {
        limbsCoeff[i] = limbs[i];  // copia del NativePoly
        if (limbsCoeff[i].GetFormat() == Format::COEFFICIENT) {
            limbsCoeff[i].SetFormat(Format::EVALUATION);
        }
    }
    return s;
}


BigInteger checkSum(
    const Ciphertext<DCRTPoly>& ct,
    const BigInteger& alpha,
    const BigInteger& q_check,
    uint32_t polyIndex = 0)
{
    // Copia para no mutar el ciphertext original
    DCRTPoly dcrtpoly = ct->GetElements()[polyIndex];
    dcrtpoly.SetFormat(Format::COEFFICIENT);

    auto vplain = dcrtpoly.CRTInterpolate();
    uint32_t ringDim = vplain.GetLength();

    BigInteger s(0);
    BigInteger power(1);

    for (uint32_t j = 0; j < ringDim; j++) {
        BigInteger coeff = vplain[j].Mod(q_check);
        s     = (s + coeff * power).Mod(q_check);
        power = (power * alpha).Mod(q_check);
    }

    return s;
}

int main() {

    uint32_t multDepth = 10;


    uint32_t scaleModSize = 50;

    uint32_t batchSize = 8;

    uint32_t logN = 8;

    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetBatchSize(batchSize);
    parameters.SetSecurityLevel(HEStd_NotSet);

    parameters.SetRingDim(1 << logN);
    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
    auto cfg = SDCConfigHelper::MakeConfig(
        false,                            // enableDetection
        SecretKeyAttackMode::CompleteInjection,   // attackMode
        5.0                              // thresholdBits
    );
    SDCConfigHelper::SetGlobalConfig(cfg);
    // Enable the features that you wish to use
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);
    std::cout << "CKKS scheme is using ring dimension " << cc->GetRingDimension() << std::endl << std::endl;

    auto keys = cc->KeyGen();

    uint32_t indexConj = 2 * cc->GetRingDimension() - 1;
    cc->EvalAutomorphismKeyGen(keys.secretKey, {indexConj});
    cc->EvalSumKeyGen(keys.secretKey);
    cc->EvalMultKeyGen(keys.secretKey);

    // Inputs
    std::vector<double> x1 = {0.25, 0.5, 0.75, 1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> x0 = {5.0, 0.25, 1.0, 0.20, 1.75, 0.60, 1.0, 3.0};
    // Encoding as plaintexts
    Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(x1);
    Plaintext ptxt0 = cc->MakeCKKSPackedPlaintext(x0);

    std::cout << "Input x1: " << ptxt1 << std::endl;

    // Encrypt the encoded vectors
    auto c0 = cc->Encrypt(keys.publicKey, ptxt0);
    auto c1 = cc->Encrypt(keys.publicKey, ptxt1);
    bool isGood = check(cc, c1, batchSize);
   // std::cout << isGood << std::endl;


    BigInteger alpha("123456789101112131415");  // punto fijo, guardalo
    BigInteger q_check("998244353");            // primo de checksum tuyo

    // Checksums de c
    BigInteger s_c0 = checkSum(c0,  alpha, q_check, 0);  // polyIndex=0 → c0
    BigInteger s_c1 = checkSum(c0,  alpha, q_check, 1);  // polyIndex=1 → c1

    // Checksums de c'
    BigInteger s_c0_prime = checkSum(c1, alpha, q_check, 0);
    BigInteger s_c1_prime = checkSum(c1, alpha, q_check, 1);

    // Checksum esperado de c'' = c + c'
    BigInteger s_c0_expected = (s_c0 + s_c0_prime).Mod(q_check);
    BigInteger s_c1_expected = (s_c1 + s_c1_prime).Mod(q_check);

    // Verificás
    auto c_add = cc->EvalAdd(c0, c1);
    BigInteger s_c0_actual = checkSum(c_add, alpha, q_check, 0);
    BigInteger s_c1_actual = checkSum(c_add, alpha, q_check, 1);

    std::cout << s_c0_actual << " " << s_c0_expected << std::endl;
     std::cout << s_c1_actual << " " << s_c1_expected << std::endl;
    assert(s_c0_actual == s_c0_expected);
    assert(s_c1_actual == s_c1_expected);

std::cout <<  std::endl;
    auto cAdd = cc->EvalSub(c0, c1);
    auto s_bAdd = checkSum(cAdd, alpha, q_check, 0);  // componente b
    auto s_aAdd = checkSum(cAdd, alpha, q_check, 1);
    std::cout << "s_bAdd: " << s_bAdd << std::endl;
    std::cout << "s_aAdd: " << s_aAdd << std::endl;   
std::cout <<  std::endl;
 //   auto c1 = cc->Encrypt(keys.publicKey, ptxt1);
    NativeInteger& x = c1->GetElements()[0].GetAllElements()[0][0];
    uint64_t val = x.ConvertToInt();  // Extrae como uint64_t
    val ^= (1ULL << 1);               // Aplica XOR
    x = NativeInteger(val);
    cAdd = cc->EvalSub(c0, c1);
    s_bAdd = checkSum(cAdd, alpha, q_check, 0);  // componente b
    s_aAdd = checkSum(cAdd, alpha, q_check, 1);
    std::cout << "s_bAdd: " << s_bAdd << std::endl;
    std::cout << "s_aAdd: " << s_aAdd << std::endl;   


    isGood = check(cc, c1, batchSize);

  //  std::cout << isGood << std::endl;
    auto s_b = checkSum(c1, alpha, q_check, 0);  // componente b
   auto  s_a = checkSum(c1, alpha, q_check, 1);

    std::cout << "s_b: " << s_b << std::endl;
    std::cout << "s_a: " << s_a << std::endl;   

    auto evalConjKeyMap = cc->GetEvalAutomorphismKeyMap(c1->GetKeyTag());

    auto cConj1         = cc->EvalAutomorphism(c1, indexConj, evalConjKeyMap);

    auto cSubC = cc->EvalSub(c1, cConj1);

    auto cStd = StdDevCKKS(cc, cSubC, batchSize);

    auto diff = cc->EvalSub(cStd, 0.32);

    // Step 5: Decryption and output
    Plaintext result;

    std::cout.precision(8);

    std::cout << std::endl << "Results of homomorphic computations: " << std::endl;

    cc->Decrypt(keys.secretKey, c1, &result);
    bool detected = SDCConfigHelper::WasSDCDetected(result);
    if (detected) {
        std::cout << "SDC detectado!" << std::endl;
    }
   // bool sdcDetected = ckksPT->GetDecodeSDCState().lastSDCDetected;
    result->SetLength(batchSize);
    std::cout << "x1 = " << result;
    std::cout << "Estimated precision in bits: " << result->GetLogPrecision() << std::endl;
    cc->Decrypt(keys.secretKey, cConj1, &result);
    result->SetLength(batchSize);
    std::cout << "x1 conjugated = " << result;
    // Decrypt the result of subtraction
    cc->Decrypt(keys.secretKey, cSubC, &result);
    result->SetLength(batchSize);
    std::cout << "x1 - x2 = " << result;

    cc->Decrypt(keys.secretKey, cStd, &result);
    result->SetLength(batchSize);
    std::cout << "std = " << result;


    cc->Decrypt(keys.secretKey, diff, &result);
    result->SetLength(batchSize);
    std::cout << "diff = " << result;
    return 0;
}
