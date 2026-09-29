#pragma once
// Waving flags / streamers (flg_* instances): development recovery of the
// original flag manager. Everything here is procedural (a sine-table wave
// sum over an 8 x h grid built from the flag model's four corner vertices);
// there is no cloth physics.
//
//  * Stage program (builtin 12, 0x2FC9C8, defaults 0x4FB540) builds a 0x68-byte
//    argument block; 0x34AC88 turns it into the 0x58-byte parameter block.
//  * Flag entity (type 10, vtable 0x48FC10, ctor 0x34ADD8) clears instance
//    runtime bit 1 (static draw) and registers with the flag manager
//    (0x34C548): 15 cloth slots (+0x20, stride 0x188) keyed by model id and
//    memcmp of the parameter block; the first instance builds the grid
//    (0x34B228), later ones share it. Entity dtor unregisters (0x34C600 ->
//    0x34B168, swap-remove; last one frees mesh/verts via 0x34B7B8).
//  * Manager tick (vtable 0x48FB80 +0x10 = 0x34C668, once per game tick):
//    one-second random-walk wind in [0,1] (amplitude 0.15/0.30/0.45 by mode
//    0x2D1BA0; Snow Jam: 0.15), then 0x34B818 per occupied slot: phase
//    advance, grid recompute (0x34BCA0) only when frame%2 == slot parity,
//    UV scroll.
//  * Draw (0x34C7F8 -> 0x34B9B0): each registered instance draws the shared
//    dynamic mesh with its own instance matrix; h-1 triangle strips of 2w
//    vertices (row r, row r+1 interleaved; 0x3856B8/0x3852E8). Positions are
//    in model space.
// Arithmetic: EE scalar FPU (MUL chop via terrain_original::mul, ADD/SUB with
// the PCSX2 guard bit, DIV nearest, CVT.W.S chop) under OriginalRounding.
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <vector>
#include "original_float.hpp"
#include "terrain_contact_math.hpp"

namespace ssx {

// 0x504FB8: 640-entry table written by 0x392DF0 (sinf 0x31BF60 of i*2pi/512);
// entry i+128 is the cosine. Bits verified against the recompiled generator
// (tests/flag_cloth_reference.cpp) and every race snapshot.
inline constexpr std::array<uint32_t,640> originalFlagSineTableBits{
    0x00000000u,0x3C490E8Fu,0x3CC90AAFu,0x3D16C32Bu,0x3D48FB2Fu,0x3D7B2B73u,0x3D96A904u,0x3DAFB67Fu,0x3DC8BD35u,0x3DE1BC2Du,
    0x3DFAB271u,0x3E09CF85u,0x3E164082u,0x3E22ABB5u,0x3E2F10A1u,0x3E3B6ECEu,0x3E47C5C1u,0x3E541500u,0x3E605C12u,0x3E6C9A7Eu,
    0x3E78CFCBu,0x3E827DC0u,0x3E888E92u,0x3E8E9A21u,0x3E94A031u,0x3E9AA085u,0x3EA09AE4u,0x3EA68F11u,0x3EAC7CD2u,0x3EB263EEu,
    0x3EB84429u,0x3EBE1D49u,0x3EC3EF15u,0x3EC9B952u,0x3ECF7BC9u,0x3ED53640u,0x3EDAE87Fu,0x3EE0924Eu,0x3EE63373u,0x3EEBCBBAu,
    0x3EF15AE9u,0x3EF6E0C9u,0x3EFC5D27u,0x3F00E7E3u,0x3F039C3Cu,0x3F064B82u,0x3F08F59Au,0x3F0B9A6Bu,0x3F0E39D9u,0x3F10D3CCu,
    0x3F13682Au,0x3F15F6D8u,0x3F187FBFu,0x3F1B02C5u,0x3F1D7FD0u,0x3F1FF6CAu,0x3F226798u,0x3F24D224u,0x3F273655u,0x3F299413u,
    0x3F2BEB49u,0x3F2E3BDDu,0x3F3085BAu,0x3F32C8C8u,0x3F3504F3u,0x3F373A22u,0x3F396841u,0x3F3B8F3Bu,0x3F3DAEF9u,0x3F3FC767u,
    0x3F41D870u,0x3F43E200u,0x3F45E403u,0x3F47DE65u,0x3F49D112u,0x3F4BBBF7u,0x3F4D9F01u,0x3F4F7A1Fu,0x3F514D3Cu,0x3F531849u,
    0x3F54DB31u,0x3F5695E4u,0x3F584852u,0x3F59F269u,0x3F5B941Au,0x3F5D2D52u,0x3F5EBE05u,0x3F604621u,0x3F61C596u,0x3F633C59u,
    0x3F64AA59u,0x3F660F86u,0x3F676BD7u,0x3F68BF3Bu,0x3F6A09A6u,0x3F6B4B0Bu,0x3F6C835Eu,0x3F6DB293u,0x3F6ED89Du,0x3F6FF573u,
    0x3F710908u,0x3F721352u,0x3F731447u,0x3F740BDDu,0x3F74FA0Bu,0x3F75DEC6u,0x3F76BA07u,0x3F778BC5u,0x3F7853F7u,0x3F791298u,
    0x3F79C79Du,0x3F7A7302u,0x3F7B14BEu,0x3F7BACCDu,0x3F7C3B28u,0x3F7CBFC9u,0x3F7D3AACu,0x3F7DABCCu,0x3F7E1324u,0x3F7E70B0u,
    0x3F7EC46Du,0x3F7F0E58u,0x3F7F4E6Eu,0x3F7F84ABu,0x3F7FB10Fu,0x3F7FD398u,0x3F7FEC43u,0x3F7FFB11u,0x3F800000u,0x3F7FFB11u,
    0x3F7FEC44u,0x3F7FD398u,0x3F7FB110u,0x3F7F84ACu,0x3F7F4E6Eu,0x3F7F0E59u,0x3F7EC46Eu,0x3F7E70B1u,0x3F7E1324u,0x3F7DABCDu,
    0x3F7D3AADu,0x3F7CBFCAu,0x3F7C3B29u,0x3F7BACCEu,0x3F7B14C0u,0x3F7A7303u,0x3F79C79Fu,0x3F791299u,0x3F7853F9u,0x3F778BC7u,
    0x3F76BA09u,0x3F75DEC8u,0x3F74FA0Cu,0x3F740BDFu,0x3F731449u,0x3F721354u,0x3F71090Au,0x3F6FF575u,0x3F6ED89Fu,0x3F6DB295u,
    0x3F6C8360u,0x3F6B4B0Du,0x3F6A09A9u,0x3F68BF3Eu,0x3F676BDAu,0x3F660F8Au,0x3F64AA5Bu,0x3F633C5Cu,0x3F61C599u,0x3F604625u,
    0x3F5EBE09u,0x3F5D2D56u,0x3F5B941Du,0x3F59F26Cu,0x3F584855u,0x3F5695E7u,0x3F54DB35u,0x3F53184Cu,0x3F514D40u,0x3F4F7A22u,
    0x3F4D9F05u,0x3F4BBBFAu,0x3F49D117u,0x3F47DE69u,0x3F45E407u,0x3F43E204u,0x3F41D874u,0x3F3FC76Au,0x3F3DAEFCu,0x3F3B8F40u,
    0x3F396847u,0x3F373A27u,0x3F3504F7u,0x3F32C8CDu,0x3F3085BEu,0x3F2E3BE1u,0x3F2BEB4Fu,0x3F299419u,0x3F27365Au,0x3F24D229u,
    0x3F22679Du,0x3F1FF6CEu,0x3F1D7FD4u,0x3F1B02CBu,0x3F187FC5u,0x3F15F6DEu,0x3F13682Fu,0x3F10D3D1u,0x3F0E39DDu,0x3F0B9A71u,
    0x3F08F5A0u,0x3F064B88u,0x3F039C42u,0x3F00E7E9u,0x3EFC5D2Fu,0x3EF6E0D2u,0x3EF15AF7u,0x3EEBCBC7u,0x3EE63380u,0x3EE09259u,
    0x3EDAE889u,0x3ED53649u,0x3ECF7BD1u,0x3EC9B960u,0x3EC3EF22u,0x3EBE1D55u,0x3EB84434u,0x3EB263F8u,0x3EAC7CDCu,0x3EA68F19u,
    0x3EA09AF2u,0x3E9AA093u,0x3E94A03Du,0x3E8E9A2Cu,0x3E888E9Cu,0x3E827DC9u,0x3E78CFEAu,0x3E6C9A9Bu,0x3E605C2Du,0x3E541518u,
    0x3E47C5D7u,0x3E3B6EE2u,0x3E2F10B3u,0x3E22ABD4u,0x3E16409Fu,0x3E09CFA0u,0x3DFAB2A1u,0x3DE1BC58u,0x3DC8BD5Bu,0x3DAFB6A1u,
    0x3D96A940u,0x3D7B2BE2u,0x3D48FB95u,0x3D16C388u,0x3CC90B55u,0x3C490FB5u,0x34800000u,0xBC490CB5u,0xBCC909D5u,0xBD16C2C8u,
    0xBD48FAD5u,0xBD7B2B23u,0xBD96A8E1u,0xBDAFB641u,0xBDC8BCFCu,0xBDE1BBF9u,0xBDFAB242u,0xBE09CF70u,0xBE164070u,0xBE22ABA4u,
    0xBE2F1083u,0xBE3B6EB2u,0xBE47C5A8u,0xBE5414E9u,0xBE605BFEu,0xBE6C9A6Cu,0xBE78CFBBu,0xBE827DB1u,0xBE888E85u,0xBE8E9A15u,
    0xBE94A026u,0xBE9AA07Cu,0xBEA09ADBu,0xBEA68F03u,0xBEAC7CC5u,0xBEB263E2u,0xBEB8441Du,0xBEBE1D3Fu,0xBEC3EF0Bu,0xBEC9B94Au,
    0xBECF7BBBu,0xBED53634u,0xBEDAE874u,0xBEE09243u,0xBEE6336Au,0xBEEBCBB2u,0xBEF15AE2u,0xBEF6E0BDu,0xBEFC5D1Au,0xBF00E7DEu,
    0xBF039C37u,0xBF064B7Eu,0xBF08F596u,0xBF0B9A67u,0xBF0E39D3u,0xBF10D3C7u,0xBF136825u,0xBF15F6D4u,0xBF187FBBu,0xBF1B02C2u,
    0xBF1D7FCBu,0xBF1FF6C4u,0xBF226794u,0xBF24D220u,0xBF273651u,0xBF299411u,0xBF2BEB46u,0xBF2E3BD8u,0xBF3085B5u,0xBF32C8C4u,
    0xBF3504EFu,0xBF373A20u,0xBF396840u,0xBF3B8F39u,0xBF3DAEF5u,0xBF3FC764u,0xBF41D86Bu,0xBF43E1FBu,0xBF45E3FEu,0xBF47DE61u,
    0xBF49D10Eu,0xBF4BBBF4u,0xBF4D9EFFu,0xBF4F7A1Du,0xBF514D3Au,0xBF531847u,0xBF54DB30u,0xBF5695DFu,0xBF58484Eu,0xBF59F265u,
    0xBF5B9416u,0xBF5D2D4Fu,0xBF5EBE02u,0xBF60461Eu,0xBF61C595u,0xBF633C57u,0xBF64AA57u,0xBF660F86u,0xBF676BD6u,0xBF68BF3Au,
    0xBF6A09A6u,0xBF6B4B08u,0xBF6C835Bu,0xBF6DB290u,0xBF6ED89Bu,0xBF6FF571u,0xBF710906u,0xBF721350u,0xBF731446u,0xBF740BDCu,
    0xBF74FA09u,0xBF75DEC5u,0xBF76BA06u,0xBF778BC5u,0xBF7853F6u,0xBF791296u,0xBF79C79Cu,0xBF7A7300u,0xBF7B14BDu,0xBF7BACCCu,
    0xBF7C3B27u,0xBF7CBFC8u,0xBF7D3AABu,0xBF7DABCBu,0xBF7E1323u,0xBF7E70B0u,0xBF7EC46Du,0xBF7F0E58u,0xBF7F4E6Du,0xBF7F84ABu,
    0xBF7FB10Fu,0xBF7FD398u,0xBF7FEC43u,0xBF7FFB11u,0xBF800000u,0xBF7FFB11u,0xBF7FEC44u,0xBF7FD398u,0xBF7FB110u,0xBF7F84ACu,
    0xBF7F4E6Eu,0xBF7F0E59u,0xBF7EC46Fu,0xBF7E70B2u,0xBF7E1325u,0xBF7DABCDu,0xBF7D3AAEu,0xBF7CBFCBu,0xBF7C3B2Au,0xBF7BACCFu,
    0xBF7B14C0u,0xBF7A7304u,0xBF79C79Fu,0xBF791299u,0xBF7853F9u,0xBF778BC9u,0xBF76BA0Bu,0xBF75DECAu,0xBF74FA0Eu,0xBF740BE1u,
    0xBF73144Bu,0xBF721356u,0xBF71090Bu,0xBF6FF576u,0xBF6ED8A1u,0xBF6DB296u,0xBF6C8361u,0xBF6B4B0Eu,0xBF6A09ACu,0xBF68BF41u,
    0xBF676BDDu,0xBF660F8Du,0xBF64AA5Eu,0xBF633C5Eu,0xBF61C59Cu,0xBF604626u,0xBF5EBE0Au,0xBF5D2D57u,0xBF5B941Eu,0xBF59F26Du,
    0xBF584856u,0xBF5695E8u,0xBF54DB39u,0xBF531850u,0xBF514D44u,0xBF4F7A26u,0xBF4D9F09u,0xBF4BBBFEu,0xBF49D118u,0xBF47DE6Bu,
    0xBF45E409u,0xBF43E205u,0xBF41D875u,0xBF3FC76Cu,0xBF3DAEFDu,0xBF3B8F44u,0xBF39684Bu,0xBF373A2Bu,0xBF3504FCu,0xBF32C8D2u,
    0xBF3085C4u,0xBF2E3BE7u,0xBF2BEB52u,0xBF29941Du,0xBF27365Du,0xBF24D22Cu,0xBF2267A0u,0xBF1FF6D1u,0xBF1D7FD7u,0xBF1B02D2u,
    0xBF187FCCu,0xBF15F6E4u,0xBF136835u,0xBF10D3D7u,0xBF0E39E4u,0xBF0B9A75u,0xBF08F5A4u,0xBF064B8Bu,0xBF039C45u,0xBF00E7ECu,
    0xBEFC5D36u,0xBEF6E0D9u,0xBEF15B05u,0xBEEBCBD5u,0xBEE6338Eu,0xBEE09267u,0xBEDAE898u,0xBED53658u,0xBECF7BE0u,0xBEC9B968u,
    0xBEC3EF29u,0xBEBE1D5Cu,0xBEB8443Bu,0xBEB26400u,0xBEAC7CE3u,0xBEA68F21u,0xBEA09B01u,0xBE9AA0A2u,0xBE94A04Cu,0xBE8E9A3Cu,
    0xBE888EACu,0xBE827DD8u,0xBE78CFF9u,0xBE6C9AAAu,0xBE605C3Cu,0xBE541528u,0xBE47C5E7u,0xBE3B6EF1u,0xBE2F10C2u,0xBE22ABF3u,
    0xBE1640BFu,0xBE09CFC0u,0xBDFAB2E1u,0xBDE1BC98u,0xBDC8BD9Bu,0xBDAFB6E1u,0xBD96A960u,0xBD7B2C22u,0xBD48FBD5u,0xBD16C3C8u,
    0xBCC90BD5u,0xBC4910B5u,0xB5000000u,0x3C490AB5u,0x3CC908D5u,0x3D16C248u,0x3D48FA55u,0x3D7B2AA3u,0x3D96A8A1u,0x3DAFB621u,
    0x3DC8BCDCu,0x3DE1BBD9u,0x3DFAB222u,0x3E09CF61u,0x3E164060u,0x3E22AB94u,0x3E2F1064u,0x3E3B6E93u,0x3E47C588u,0x3E5414CAu,
    0x3E605BDFu,0x3E6C9A4Du,0x3E78CF9Cu,0x3E827DAAu,0x3E888E7Eu,0x3E8E9A0Eu,0x3E94A01Eu,0x3E9AA074u,0x3EA09AD4u,0x3EA68EF3u,
    0x3EAC7CB6u,0x3EB263D3u,0x3EB8440Fu,0x3EBE1D30u,0x3EC3EEFDu,0x3EC9B93Cu,0x3ECF7BB4u,0x3ED5362Cu,0x3EDAE86Cu,0x3EE0923Cu,
    0x3EE63363u,0x3EEBCBABu,0x3EF15ADBu,0x3EF6E0AFu,0x3EFC5D0Du,0x3F00E7D7u,0x3F039C31u,0x3F064B77u,0x3F08F590u,0x3F0B9A61u,
    0x3F0E39D0u,0x3F10D3C3u,0x3F136822u,0x3F15F6D1u,0x3F187FB8u,0x3F1B02BEu,0x3F1D7FC4u,0x3F1FF6BEu,0x3F22678Du,0x3F24D21Au,
    0x3F27364Bu,0x3F29940Bu,0x3F2BEB40u,0x3F2E3BD5u,0x3F3085B2u,0x3F32C8C1u,0x3F3504ECu,0x3F373A20u,0x3F396840u,0x3F3B8F39u,
    0x3F3DAEF3u,0x3F3FC761u,0x3F41D86Bu,0x3F43E1FBu,0x3F45E3FEu,0x3F47DE61u,0x3F49D10Eu,0x3F4BBBF4u,0x3F4D9EFFu,0x3F4F7A1Du,
    0x3F514D3Au,0x3F531847u,0x3F54DB30u,0x3F5695DFu,0x3F58484Eu,0x3F59F265u,0x3F5B9416u,0x3F5D2D4Fu,0x3F5EBE02u,0x3F60461Eu,
    0x3F61C595u,0x3F633C57u,0x3F64AA57u,0x3F660F86u,0x3F676BD6u,0x3F68BF3Au,0x3F6A09A6u,0x3F6B4B08u,0x3F6C835Bu,0x3F6DB290u,
    0x3F6ED89Bu,0x3F6FF571u,0x3F710906u,0x3F721350u,0x3F731446u,0x3F740BDCu,0x3F74FA09u,0x3F75DEC5u,0x3F76BA06u,0x3F778BC5u,
    0x3F7853F6u,0x3F791296u,0x3F79C79Cu,0x3F7A7300u,0x3F7B14BDu,0x3F7BACCCu,0x3F7C3B27u,0x3F7CBFC8u,0x3F7D3AABu,0x3F7DABCBu,
    0x3F7E1323u,0x3F7E70B0u,0x3F7EC46Du,0x3F7F0E58u,0x3F7F4E6Du,0x3F7F84ABu,0x3F7FB10Fu,0x3F7FD398u,0x3F7FEC43u,0x3F7FFB11u,
};
inline float originalFlagSine(unsigned index){return std::bit_cast<float>(originalFlagSineTableBits.at(index));}

inline constexpr int originalFlagWidth=8;           // 0x34B228: +0x64 always 8
inline constexpr int originalFlagSlots=15;          // 0x34C548 loop bound
inline constexpr int originalFlagMaxInstances=60;   // +0x94..+0x180 list (0x34AF38 clears 0x3C words)

// Builtin-12 argument block (0x68 bytes, key k at +4k). Defaults from 0x2FC9C8.
struct OriginalFlagArguments {
    std::array<uint32_t,26> words{};
    static OriginalFlagArguments defaults(){
        OriginalFlagArguments a;auto f=[&](unsigned k,float v){a.words[k]=std::bit_cast<uint32_t>(v);};
        a.words[0]=0xFFFFFFFFu;a.words[1]=1;a.words[2]=0;a.words[3]=0;
        f(4,1);f(5,25);f(6,1);f(7,1);f(8,0);f(9,1);f(10,1);f(11,0);f(12,1);f(13,1);f(14,0);f(15,1);
        for(unsigned k=16;k<=24;k++)f(k,0);a.words[25]=0;return a;
    }
    float real(unsigned k)const{return std::bit_cast<float>(words.at(k));}
};

// 0x58-byte parameter block (cloth +0x00..+0x57), the exact memcmp key.
struct OriginalFlagParameters {
    uint8_t attachStart=0,attachEnd=0,pinFirstRow=0,drawMode=0;  // +0..+3 (args +4,+8,+C,+64 != 0)
    std::array<float,4> speed{};      // +0x04 per tick (args / fps)
    std::array<float,4> amplitude{};  // +0x14
    std::array<float,4> frequency{};  // +0x24
    std::array<float,3> calm{};       // +0x34 offset at strength 0 (weighted by 1-strength)
    std::array<float,3> gust{};       // +0x40 offset at strength 1
    float minimumStrength=0;          // +0x4C
    std::array<float,2> uvSpeed{};    // +0x50 per tick
    bool operator==(const OriginalFlagParameters&)const=default;
};
static_assert(sizeof(OriginalFlagParameters)==0x58);

// 0x34AC88. fps = [[gp+0x2A74]+0x10] (60 in the NTSC race).
inline OriginalFlagParameters originalFlagParameters(const OriginalFlagArguments& a,int32_t fps){
    OriginalRounding rounding;using terrain_original::mul;OriginalFlagParameters p;
    p.attachStart=a.words[1]!=0;p.attachEnd=a.words[2]!=0;p.pinFirstRow=a.words[3]!=0;p.drawMode=a.words[25]!=0;
    for(unsigned k=0;k<3;k++){p.calm[k]=a.real(16+k);p.gust[k]=a.real(19+k);}
    p.minimumStrength=a.real(22);
    auto perTick=[&](unsigned k){return mul(a.real(k),originalScalarDivide(1.f,float(fps)));};
    p.uvSpeed={perTick(23),perTick(24)};
    for(unsigned i=0;i<4;i++){p.speed[i]=perTick(4+3*i);p.amplitude[i]=a.real(5+3*i);p.frequency[i]=a.real(6+3*i);}
    return p;
}

// Four corner vertices read by the renderer (vtable +0x308 = 0x380380) from
// the model's first mesh: strip order c0 c1 c2 c3; colour is (bit15, r/31, g/31, b/31).
struct OriginalFlagCorners {
    std::array<std::array<float,3>,4> position{};
    std::array<std::array<float,2>,4> uv{};
    std::array<std::array<float,4>,4> colour{};
};

using OriginalFlagVec3=std::array<float,3>;

// 0x3177F0 float: (word & 0x7FFFFF | 0x3F800000) - 1.
inline float originalFlagUnitRandom(uint32_t word){
    return originalScalarSubtract(std::bit_cast<float>((word&0x7FFFFFu)|0x3F800000u),1.f);
}

struct OriginalFlagCloth {
    OriginalFlagParameters parameters;
    uint32_t model=0;                  // +0x58 ([instance+0x80]+0)
    int count=0;                       // +0x5C registered instances
    bool mesh=false;                   // +0x60 != 0
    int width=0,height=0;              // +0x64/+0x68
    float widthSpan=0,heightSpan=0;    // +0x6C/+0x70
    std::vector<OriginalFlagVec3> base;// +0x74 model-space rest grid
    std::array<float,4> phase{};       // +0x78
    std::array<float,2> uvOffset{};    // +0x88
    int parity=0;                      // +0x90
    std::vector<uint32_t> instances;   // +0x94 (instance ids, for the port)
    // Grid attributes handed to the mesh once (0x34B228 -> mesh vtable +0x30).
    std::vector<std::array<float,4>> colour;   // (1, r, g, b)
    std::vector<std::array<float,2>> uv;
};

// 0x34BCA0: animated model-space positions (width*height, row-major).
inline std::vector<OriginalFlagVec3> originalFlagVertices(const OriginalFlagCloth& c,float wind){
    OriginalRounding rounding;using terrain_original::mul;
    auto add=originalScalarAdd;auto sub=originalScalarSubtract;auto div=originalScalarDivide;
    const auto& p=c.parameters;const int w=c.width,h=c.height;
    if(w<=0||h<=0||int(c.base.size())<w*h||w>64)throw std::runtime_error("Invalid flag grid");
    const bool both=p.attachStart&&p.attachEnd;            // s2
    const bool free=!p.attachStart&&!p.attachEnd;          // s1
    const float twoPi=std::bit_cast<float>(0x40C90FDBu),toIndex=std::bit_cast<float>(0x42A2F983u),
        negTwoPi=std::bit_cast<float>(0xC0C90FDBu),threshold=std::bit_cast<float>(0x3C23D70Au);
    const float strength=add(p.minimumStrength,mul(wind,sub(1.f,p.minimumStrength)));
    std::array<float,4> phase,amp;
    for(unsigned i=0;i<4;i++){phase[i]=mul(c.phase[i],negTwoPi);amp[i]=mul(p.amplitude[i],strength);}
    auto lookup=[&](float angle,float& s,float& co){
        const int32_t n=int32_t(std::trunc(mul(angle,toIndex)));   // CVT.W.S (chop)
        const unsigned i=unsigned(n)&0x1FFu;s=originalFlagSine(i);co=originalFlagSine(i+0x80);
    };
    std::vector<OriginalFlagVec3> column(w);
    float sumA=0,sumB=0,columnValue=0;
    for(int a2=0;a2<w;a2++){
        int a0=a2;float f3=div(columnValue,c.widthSpan);const float u=f3;
        if(free)f3=1.f;
        else if(both){if(.5f<f3)f3=sub(1.f,f3);}
        else if(p.attachEnd==1)a0=w-a2-1;
        const float f5=mul(u,twoPi);float s,co;
        lookup(add(phase[0],mul(f5,p.frequency[0])),s,co);
        if(!both)sumA=add(sumA,mul(mul(amp[0],f3),co));
        OriginalFlagVec3 v{mul(mul(amp[0],f3),s),sumA,0.f};
        lookup(add(phase[1],mul(f5,p.frequency[1])),s,co);
        if(!both)sumB=add(sumB,mul(mul(amp[1],f3),co));
        v[0]=add(v[0],mul(mul(amp[1],f3),s));v[1]=add(v[1],sumB);v[2]=add(v[2],0.f);
        const float calmWeight=sub(1.f,strength);
        for(unsigned k=0;k<3;k++)v[k]=add(v[k],add(mul(mul(p.gust[k],f3),strength),mul(mul(p.calm[k],f3),calmWeight)));
        column[a0]=v;columnValue=add(columnValue,1.f);
    }
    std::vector<OriginalFlagVec3> out(size_t(w)*h);
    size_t vertex=0;float rowValue=0;
    for(int row=0;row<h;row++){
        float colValue=0;
        for(int col=0;col<w;col++){
            float f8=div(colValue,c.widthSpan);const float f9=div(rowValue,c.heightSpan);float f10=f8;
            if(both){if(.5f<f8)f8=sub(1.f,f8);}
            else if(p.attachEnd==1){f8=sub(1.f,f8);f10=f8;}
            else if(free)f8=1.f;
            if(p.pinFirstRow&&f9==0.f){
                // Original quirk: the copy branch skips the vertex/pointer increments.
                out[vertex]=c.base[vertex];colValue=add(colValue,1.f);continue;
            }
            const auto& b=c.base[vertex];const auto& o=column[col];
            OriginalFlagVec3 r{add(b[0],o[0]),add(b[1],o[1]),add(b[2],o[2])};
            if(0.f<f10&&threshold<amp[2]){
                float s,co;const float half=mul(add(f10,f9),.5f),mid=mul(add(f8,f9),.5f);
                lookup(add(phase[2],mul(half,mul(p.frequency[2],twoPi))),s,co);
                r[0]=add(r[0],mul(mul(amp[2],f8),s));r[1]=add(r[1],mul(mul(amp[2],mid),co));r[2]=add(r[2],mul(mul(amp[2],f9),s));
            }
            if(0.f<f10&&threshold<amp[3]){
                float s,co;const float half=mul(add(f10,f9),.5f),mid=mul(add(f8,f9),.5f);
                lookup(add(phase[3],mul(half,mul(p.frequency[3],twoPi))),s,co);
                r[0]=add(r[0],mul(mul(amp[3],f8),s));r[1]=add(r[1],mul(mul(amp[3],mid),co));r[2]=add(r[2],mul(mul(amp[3],sub(1.f,f9)),s));
            }
            out[vertex++]=r;colValue=add(colValue,1.f);
        }
        rowValue=add(rowValue,1.f);
    }
    return out;
}

// 0x34B228 (after the slot's parameter block/model are set): random phases
// (four 0x3177F0 draws), height 2 when both vertical waves are off (< 0.01),
// bilinear rest grid / UV / colour (alpha forced to 1), then the first
// vertex computation with the manager's current wind.
inline std::vector<OriginalFlagVec3> originalFlagBuild(OriginalFlagCloth& c,const OriginalFlagCorners& k,
        const std::function<uint32_t()>& random,float wind){
    {
        OriginalRounding rounding;using terrain_original::mul;auto add=originalScalarAdd;auto sub=originalScalarSubtract;auto div=originalScalarDivide;
        for(auto& x:c.phase)x=originalFlagUnitRandom(random());
        c.uvOffset={0,0};
        const float limit=std::bit_cast<float>(0x3C23D70Au);
        c.width=originalFlagWidth;c.height=c.parameters.amplitude[2]<limit&&c.parameters.amplitude[3]<limit?2:5;
        c.widthSpan=float(c.width-1);c.heightSpan=float(c.height-1);c.mesh=true;
        const int n=c.width*c.height;c.base.assign(n,{});c.uv.assign(n,{});c.colour.assign(n,{});
        int vertex=0;
        for(int row=0;row<c.height;row++)for(int col=0;col<c.width;col++,vertex++){
            const float u=div(float(col),c.widthSpan),v=div(float(row),c.heightSpan),iu=sub(1.f,u),iv=sub(1.f,v);
            for(unsigned a=0;a<3;a++){
                const float top=add(mul(k.position[0][a],iu),mul(k.position[1][a],u)),bottom=add(mul(k.position[2][a],iu),mul(k.position[3][a],u));
                c.base[vertex][a]=add(mul(top,iv),mul(bottom,v));
            }
            for(unsigned a=0;a<2;a++){
                const float top=add(mul(k.uv[0][a],iu),mul(k.uv[1][a],u)),bottom=add(mul(k.uv[2][a],iu),mul(k.uv[3][a],u));
                c.uv[vertex][a]=add(mul(top,iv),mul(bottom,v));
            }
            c.colour[vertex][0]=1.f;
            for(unsigned a=1;a<4;a++){
                const float top=add(mul(k.colour[0][a],iu),mul(k.colour[1][a],u)),bottom=add(mul(k.colour[2][a],iu),mul(k.colour[3][a],u));
                c.colour[vertex][a]=add(mul(top,iv),mul(bottom,v));
            }
        }
    }
    return originalFlagVertices(c,wind);
}

// 0x34B818. Returns true when the grid was recomputed (mesh vtable +0x18).
inline bool originalFlagTick(OriginalFlagCloth& c,float wind,int32_t frame,bool enabled,std::vector<OriginalFlagVec3>* vertices=nullptr){
    if(!enabled||c.count==0||!c.mesh)return false;
    bool updated=false;
    {
        OriginalRounding rounding;using terrain_original::mul;auto add=originalScalarAdd;auto sub=originalScalarSubtract;
        const float strength=add(c.parameters.minimumStrength,mul(wind,sub(1.f,c.parameters.minimumStrength)));
        for(unsigned i=0;i<4;i++){float x=add(c.phase[i],mul(c.parameters.speed[i],strength));if(1.f<=x)x=sub(x,1.f);c.phase[i]=x;}
    }
    if(frame%2==c.parity){auto v=originalFlagVertices(c,wind);if(vertices)*vertices=std::move(v);updated=true;}
    {
        OriginalRounding rounding;auto add=originalScalarAdd;auto sub=originalScalarSubtract;
        const float limit=std::bit_cast<float>(0x3A83126Fu);
        if(limit<c.parameters.uvSpeed[0]||limit<c.parameters.uvSpeed[1]){
            for(unsigned k=0;k<2;k++){float x=add(c.uvOffset[k],c.parameters.uvSpeed[k]);if(1.f<x)x=sub(x,1.f);c.uvOffset[k]=x;}
        }
    }
    return updated;
}

// Manager (0x34C428 ctor: base .5, delta .25, timer 0, wind 0).
struct OriginalFlagWind {float wind=0,base=.5f,delta=.25f,timer=0;};
// 0x34C668 mode argument = 0x2D1BA0() (table 0x43D950[+0x54] + 1; 1 in Snow Jam).
inline float originalFlagWindAmplitude(int mode){
    return std::bit_cast<float>(mode==2?0x3E99999Au:mode==3?0x3EE66666u:0x3E19999Au);
}
// Wind part of 0x34C668; random() is 0x3177F0 (shared visual RNG 0x4FF018), drawn once per second.
inline void originalFlagWindTick(OriginalFlagWind& s,int mode,int32_t fps,const std::function<uint32_t()>& random){
    OriginalRounding rounding;using terrain_original::mul;auto add=originalScalarAdd;auto sub=originalScalarSubtract;
    const float amplitude=originalFlagWindAmplitude(mode);
    float timer=add(s.timer,originalScalarDivide(1.f,float(fps)));s.timer=timer;
    if(1.f<=timer){
        s.timer=sub(timer,1.f);s.base=add(s.base,s.delta);
        const float low=-amplitude;const float r=originalFlagUnitRandom(random());
        float delta=add(low,mul(sub(amplitude,low),r));s.delta=delta;
        if(add(s.base,delta)<0.f)s.delta=-s.base;
        if(1.f<add(s.base,s.delta))s.delta=sub(1.f,s.base);
    }
    s.wind=add(s.base,mul(s.delta,s.timer));
}

// Whole manager: slots + registration bookkeeping (0x34C548/0x34B038/0x34B168).
struct OriginalFlagManager {
    OriginalFlagWind wind;
    std::array<OriginalFlagCloth,originalFlagSlots> slots;
    explicit OriginalFlagManager(int parityBase=0){for(int i=0;i<originalFlagSlots;i++)slots[i].parity=(parityBase+i)&1;}
    // Returns the slot index, or -1 when all 15 slots hold other cloths (the
    // original then leaves the flag undrawn: the entity already cleared bit 1).
    int registerInstance(uint32_t instance,uint32_t model,const OriginalFlagParameters& p,const OriginalFlagCorners& corners,
                         const std::function<uint32_t()>& random){
        for(int i=0;i<originalFlagSlots;i++){auto& c=slots[i];
            if(c.count&&c.model==model&&!std::memcmp(&c.parameters,&p,sizeof p)){
                if(c.count>=originalFlagMaxInstances)throw std::runtime_error("Flag instance list overflow");
                c.instances.push_back(instance);c.count++;return i;}}
        for(int i=0;i<originalFlagSlots;i++){auto& c=slots[i];
            if(c.count)continue;
            c.parameters=p;c.model=model;originalFlagBuild(c,corners,random,wind.wind);
            c.instances.assign(1,instance);c.count=1;return i;}
        return -1;
    }
    bool unregisterInstance(uint32_t instance,uint32_t model){
        for(auto& c:slots){
            if(!c.count||c.model!=model)continue;
            for(size_t k=0;k<c.instances.size();k++)if(c.instances[k]==instance){
                c.instances[k]=c.instances.back();c.instances.pop_back();
                if(--c.count==0){c.mesh=false;c.base.clear();c.uv.clear();c.colour.clear();}
                return true;}
        }
        return false;
    }
    // One game tick (0x34C668); frame = 0x2D1C98 counter (== race tick).
    void tick(int mode,int32_t fps,int32_t frame,bool enabled,const std::function<uint32_t()>& random){
        originalFlagWindTick(wind,mode,fps,random);
        for(auto& c:slots)if(c.count)originalFlagTick(c,wind.wind,frame,enabled);
    }
};

// Draw topology (0x3856B8): strip s = rows s and s+1, vertices (s,c),(s+1,c) for c=0..w-1.
inline std::vector<uint32_t> originalFlagStripIndices(int width,int height){
    std::vector<uint32_t> out;
    for(int s=0;s+1<height;s++)for(int c=0;c<width;c++){out.push_back(uint32_t(s*width+c));out.push_back(uint32_t((s+1)*width+c));}
    return out;
}
}
