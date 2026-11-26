#!/usr/bin/env python3
"""
PDA - Pengenal Notasi PREFIX, INFIX & POSTFIX
Konversi langsung dari pda.cpp (implementasi diagram pushdown automata).

Keterangan input:
  huruf/operand : A-Z, a-z, 0-9
  operator (op) : + - * /
  kurung        : ( )

Cara pakai:
  python3 pda.py                 -> contoh pengujian + mode interaktif
  python3 pda.py --tabel         -> cetak tabel transisi
  python3 pda.py "+ A B" "A B +" -> uji string dari argumen
"""

from __future__ import annotations

import sys
from collections import deque
from dataclasses import dataclass, field, replace
from enum import Enum


# ---------------------------------------------------------------------
#  Klasifikasi token input (abstraksi 'huruf' & 'op' gambar)
# ---------------------------------------------------------------------
class InputKind(Enum):
    HURUF = "huruf"
    OP = "op"
    KBUKA = "("
    KTUTUP = ")"
    EPS = "eps"


# ---------------------------------------------------------------------
#  Simbol stack (lihat legenda gambar)
# ---------------------------------------------------------------------
PONDASI = "R"   # dasar stack
KURUNG = "P"    # penanda kurung buka (infix)
OPERATOR = "O"  # penanda operator (prefix)
OPERAND = "A"   # penanda operand (postfix)

STATE_AWAL = 0    # q0
STATE_ACCEPT = 6  # q6


# ---------------------------------------------------------------------
#  Definisi satu transisi: (dari, input, pop, push, ke)
# ---------------------------------------------------------------------
@dataclass(frozen=True)
class Transisi:
    dari: int
    input: InputKind
    pop: str
    push: str  # string yang di-push (kanan = puncak); "" berarti epsilon
    ke: int


# ---------------------------------------------------------------------
#  TABEL TRANSISI - salinan setia dari diagram gambar
# ---------------------------------------------------------------------
TABEL: list[Transisi] = [
    # ---- q0 : State Awal (percabangan sesuai token pertama) ----
    Transisi(0, InputKind.HURUF, "R", "R", 2),    # q0 -> q2 : huruf, R -> R
    Transisi(0, InputKind.HURUF, "R", "RA", 5),   # q0 -> q5 : huruf, R -> RA (postfix)
    Transisi(0, InputKind.OP, "R", "RO", 1),      # q0 -> q1 : op,    R -> RO (prefix)
    Transisi(0, InputKind.KBUKA, "R", "RP", 3),   # q0 -> q3 : (,     R -> RP (infix)

    # ---- q1 : Cabang Prefix ----
    Transisi(1, InputKind.OP, "R", "RO", 1),      # loop: op,    R -> RO
    Transisi(1, InputKind.OP, "O", "OO", 1),      # loop: op,    O -> OO
    Transisi(1, InputKind.HURUF, "O", "", 1),     # loop: huruf, O -> eps
    Transisi(1, InputKind.HURUF, "R", "R", 6),    # q1 -> q6 : huruf, R -> R (ACCEPT)

    # ---- q2 : Pendeteksi Awal ----
    Transisi(2, InputKind.OP, "R", "R", 3),       # q2 -> q3 : op, R -> R (masuk infix)
    Transisi(2, InputKind.EPS, "R", "R", 6),      # q2 -> q6 : eps, R -> R (operand tunggal)

    # ---- q3 : Menunggu Operand - Infix ----
    Transisi(3, InputKind.KBUKA, "R", "RP", 3),   # loop: (, R -> RP
    Transisi(3, InputKind.KBUKA, "P", "PP", 3),   # loop: (, P -> PP
    Transisi(3, InputKind.HURUF, "R", "R", 4),    # q3 -> q4 : huruf, R -> R
    Transisi(3, InputKind.HURUF, "P", "P", 4),    # q3 -> q4 : huruf, P -> P

    # ---- q4 : Setelah Operand - Infix ----
    Transisi(4, InputKind.KTUTUP, "P", "", 4),    # loop: ), P -> eps
    Transisi(4, InputKind.OP, "R", "R", 3),       # q4 -> q3 : op, R -> R
    Transisi(4, InputKind.OP, "P", "P", 3),       # q4 -> q3 : op, P -> P
    Transisi(4, InputKind.EPS, "R", "R", 6),      # q4 -> q6 : eps, R -> R (ACCEPT)

    # ---- q5 : Cabang Postfix ----
    Transisi(5, InputKind.HURUF, "A", "AA", 5),   # loop: huruf, A -> AA
    Transisi(5, InputKind.OP, "A", "", 5),        # loop: op,    A -> eps
    Transisi(5, InputKind.EPS, "A", "", 6),       # q5 -> q6 : eps, A -> eps (ACCEPT)
]


# ---------------------------------------------------------------------
#  Nama state & keterangan untuk laporan (caption pada gambar)
# ---------------------------------------------------------------------
NAMA_STATE = {
    0: "q0",
    1: "q1",
    2: "q2",
    3: "q3",
    4: "q4",
    5: "q5",
    6: "q6",
}

KETERANGAN_STATE = {
    0: "State Awal",
    1: "Cabang Prefix",
    2: "Pendeteksi Awal",
    3: "Menunggu Operand - Infix",
    4: "Setelah Operand - Infix",
    5: "Cabang Postfix",
    6: "ACCEPT (state akhir)",
}


def nama_state(s: int) -> str:
    return NAMA_STATE.get(s, "q?")


def keterangan_state(s: int) -> str:
    return KETERANGAN_STATE.get(s, "")


# ---------------------------------------------------------------------
#  Validasi karakter input
# ---------------------------------------------------------------------
def is_huruf(c: str) -> bool:
    return c.isalpha() or c.isdigit()


def is_op(c: str) -> bool:
    return c in "+-*/"


def karakter_valid(c: str) -> bool:
    return is_huruf(c) or is_op(c) or c in "()"


def klasifikasi(c: str) -> InputKind:
    if is_huruf(c):
        return InputKind.HURUF
    if is_op(c):
        return InputKind.OP
    if c == "(":
        return InputKind.KBUKA
    return InputKind.KTUTUP


# ---------------------------------------------------------------------
#  Hasil pemeriksaan sebuah string
# ---------------------------------------------------------------------
@dataclass
class Hasil:
    diterima: bool = False
    jenis: str = "-"    # PREFIX / INFIX / POSTFIX / OPERAND TUNGGAL / "-"
    jejak: str = ""     # jalur state penerimaan (jika diterima)
    alasan: str = ""    # penjelasan jika ditolak


# ---------------------------------------------------------------------
#  Konfigurasi BFS: (state, posisi input, stack, jejak)
# ---------------------------------------------------------------------
@dataclass
class Konfigurasi:
    state: int
    pos: int
    stack: list[str] = field(default_factory=list)
    jejak: list[int] = field(default_factory=list)


# ---------------------------------------------------------------------
#  Jalankan automata terhadap string (spasi & tab diabaikan)
# ---------------------------------------------------------------------
def jalankan(input_str: str) -> Hasil:
    hasil = Hasil()

    # Saring token: buang spasi, validasi karakter
    tokens = ""
    for c in input_str:
        if c in " \t\r\n":
            continue
        if not karakter_valid(c):
            hasil.alasan = f"Karakter tidak dikenal: '{c}'"
            return hasil
        tokens += c

    simbol_token = tokens  # 1 karakter = 1 token

    antrian: deque[Konfigurasi] = deque()
    awal = Konfigurasi(state=STATE_AWAL, pos=0, stack=[PONDASI])
    antrian.append(awal)

    state_akhir_diterima = -1
    jejak_diterima: list[int] = []

    while antrian:
        cfg = antrian.popleft()

        # --- KONDISI PENERIMAAN: -----------------------------
        # berada di q6 (ACCEPT), seluruh input sudah dibaca,
        # dan stack hanya tersisa pondasi R.
        if (
            cfg.state == STATE_ACCEPT
            and cfg.pos == len(tokens)
            and len(cfg.stack) == 1
            and cfg.stack[0] == PONDASI
        ):
            state_akhir_diterima = cfg.state
            jejak_diterima = cfg.jejak
            break  # satu jalur penerimaan cukup

        # --- Coba seluruh transisi yang berlaku --------------
        for t, tr in enumerate(TABEL):
            if tr.dari != cfg.state:
                continue

            if tr.input == InputKind.EPS:
                input_cocok = cfg.pos == len(tokens)
            else:
                input_cocok = (
                    cfg.pos < len(tokens)
                    and klasifikasi(tokens[cfg.pos]) == tr.input
                )
            if not input_cocok:
                continue

            # Cek puncak stack harus sama dengan simbol pop
            if not cfg.stack or cfg.stack[-1] != tr.pop:
                continue

            # Terapkan: pindah state, pop simbol, lalu push string baru
            lanjut = replace(
                cfg,
                state=tr.ke,
                stack=cfg.stack[:-1] + list(tr.push),
                jejak=cfg.jejak + [t],
            )
            if tr.input != InputKind.EPS:
                lanjut.pos += 1  # input terkonsumsi
            antrian.append(lanjut)

    # --- Susun hasil ------------------------------------------------
    if state_akhir_diterima >= 0:
        hasil.diterima = True
        cabang = state_masuk_accept(jejak_diterima)
        hasil.jenis = (
            "PREFIX" if cabang == 1
            else "POSTFIX" if cabang == 5
            else "INFIX" if cabang == 4
            else "OPERAND TUNGGAL"
        )
        hasil.jejak = bentuk_jejak(jejak_diterima, simbol_token)
    else:
        hasil.alasan = (
            "Tidak ada jalur menuju state ACCEPT (q6); "
            "string tidak dikenali sebagai ekspresi "
            "Prefix/Infix/Postfix yang valid."
        )
    return hasil


# State sebelum transisi terakhir (masuk ke q6) -> menentukan cabang
def state_masuk_accept(jejak: list[int]) -> int:
    if not jejak:
        return -1
    return TABEL[jejak[-1]].dari


# Bentuk string jalur: q0 --[huruf 'A', R->RA]--> q5 ...
def bentuk_jejak(jejak: list[int], simbol_token: str) -> str:
    bagian = [nama_state(STATE_AWAL)]
    pos = 0
    for idx in jejak:
        tr = TABEL[idx]
        teks_push = "eps" if tr.push == "" else tr.push
        potongan = f"--[{tr.input.value}"
        if tr.input != InputKind.EPS and pos < len(simbol_token):
            potongan += f" '{simbol_token[pos]}'"
        potongan += f", {tr.pop}->{teks_push}]--> {nama_state(tr.ke)}"
        bagian.append(potongan)
        if tr.input != InputKind.EPS:
            pos += 1
    return " ".join(bagian)


# ---------------------------------------------------------------------
#  Tampilan bantuan
# ---------------------------------------------------------------------
def cetak_tabel_transisi() -> None:
    print("TABEL TRANSISI PDA (input, pop -> push)")
    print("------------------------------------------------")
    for t in TABEL:
        teks_push = "eps" if t.push == "" else t.push
        print(
            f"  {nama_state(t.dari)} -> {nama_state(t.ke)} : "
            f"{t.input.value}, {t.pop} -> {teks_push}"
        )
    print("------------------------------------------------")
    print("Penerimaan: state q6 (ACCEPT), input habis, stack = [R]")


def uji(s: str) -> None:
    h = jalankan(s)

    print("--------------------------------------------------")
    print(f'Input  : "{s}"')
    print(f"Hasil  : {'DITERIMA (ACCEPT)' if h.diterima else 'DITOLAK (REJECT)'}")
    if h.diterima:
        print(f"Notasi : {h.jenis}")
        print(f"Jalur  : {h.jejak}")
    else:
        print(f"Alasan : {h.alasan}")


# ---------------------------------------------------------------------
#  Program utama
# ---------------------------------------------------------------------
def main(argv: list[str]) -> int:
    # --tabel : cetak tabel transisi lalu keluar
    if len(argv) > 1 and argv[1] == "--tabel":
        cetak_tabel_transisi()
        return 0

    print("====================================================")
    print("  PDA - PENGENAL NOTASI PREFIX, INFIX & POSTFIX")
    print("  (implementasi diagram pushdown automata)")
    print("====================================================")
    print("Keterangan input:")
    print("  huruf/operand : A-Z, a-z, 0-9")
    print("  operator (op) : + - * /")
    print("  kurung        : ( )")
    print("  perintah      : 'tabel' = lihat tabel transisi,")
    print("                  'exit'  = keluar\n")

    # Argumen baris perintah: uji string tsb langsung
    if len(argv) > 1:
        for arg in argv[1:]:
            uji(arg)
        return 0

    # Contoh pengujian otomatis
    print("========== CONTOH PENGUJIAN OTOMATIS ==========")
    print("-- PREFIX --")
    uji("+ A B")            # valid   : +AB
    uji("- * A B C")        # valid   : (A*B)-C
    uji("+ - A B * C D")    # valid   : (A-B)+(C*D)
    uji("+ A")              # invalid : operand kurang
    uji("+ A B C")          # invalid : operand lebih
    print("-- INFIX --")
    uji("A + B")            # valid
    uji("( A + B ) * C")    # valid berkurung
    uji("A + B * C - D")    # valid
    uji("( ( A + B ) )")    # valid kurung bertingkat
    uji("A + ")             # invalid : operator menggantung
    uji("( A + B")          # invalid : kurung tidak ditutup
    uji("A + + B")          # invalid : operator ganda
    print("-- POSTFIX --")
    uji("A B +")            # valid   : AB+
    uji("A B * C -")        # valid   : (A*B)-C
    uji("A B C * + D -")    # valid   : (A+B*C)-D
    uji("A B + C")          # invalid : operand lebih
    uji("A B + +")          # invalid : operator lebih
    print("-- LAIN-LAIN --")
    uji("A")                # operand tunggal
    uji("A B")              # invalid
    print("========== AKHIR CONTOH ==========\n")

    # Mode interaktif
    while True:
        try:
            baris = input("\nEkspresi> ")
        except (EOFError, KeyboardInterrupt):
            break
        if baris in ("exit", "quit"):
            break
        if baris == "tabel":
            cetak_tabel_transisi()
            continue
        if baris == "":
            continue
        uji(baris)

    print("Selesai.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
