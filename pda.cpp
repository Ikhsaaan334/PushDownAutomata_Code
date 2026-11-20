#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#ifdef DEBUG_PDA
#include <cstdio>
#endif

class PushdownAutomata {
public:
    // ----- Klasifikasi token input (abstraksi 'huruf' & 'op' gambar) -----
    enum InputKind { HURUF, OP, KBUKA, KTUTUP, EPS };

    // ----- Simbol stack (lihat legenda gambar) -----
    static const char PONDASI   = 'R';  // dasar stack
    static const char KURUNG    = 'P';  // penanda kurung buka (infix)
    static const char OPERATOR  = 'O';  // penanda operator (prefix)
    static const char OPERAND   = 'A';  // penanda operand (postfix)

    // ----- Definisi satu transisi: (dari, input, pop, push, ke) -----
    struct Transisi {
        int       dari;
        InputKind input;
        char      pop;
        const char* push;   // string yang di-push (kanan = puncak)
        int       ke;
    };

    // ----- TABEL TRANSISI - salinan setia dari diagram gambar -----
    static const Transisi TABEL[];
    static const int JUMLAH_TRANSISI;
    static const int STATE_AWAL   = 0;   // q0
    static const int STATE_ACCEPT = 6;   // q6

    // ----- Nama state untuk laporan (caption pada gambar) -----
    static const char* namaState(int s) {
        switch (s) {
            case 0: return "q0";
            case 1: return "q1";
            case 2: return "q2";
            case 3: return "q3";
            case 4: return "q4";
            case 5: return "q5";
            case 6: return "q6";
            default: return "q?";
        }
    }
    static const char* keteranganState(int s) {
        switch (s) {
            case 0: return "State Awal";
            case 1: return "Cabang Prefix";
            case 2: return "Pendeteksi Awal";
            case 3: return "Menunggu Operand - Infix";
            case 4: return "Setelah Operand - Infix";
            case 5: return "Cabang Postfix";
            case 6: return "ACCEPT (state akhir)";
            default: return "";
        }
    }
    static const char* labelInput(InputKind k) {
        switch (k) {
            case HURUF:  return "huruf";
            case OP:     return "op";
            case KBUKA:  return "(";
            case KTUTUP: return ")";
            case EPS:    return "eps";
            default:     return "?";
        }
    }

    // ----- Validasi karakter input -----
    static bool isHuruf(char c) {
        return std::isalpha((unsigned char)c) || std::isdigit((unsigned char)c);
    }
    static bool isOp(char c) {
        return c == '+' || c == '-' || c == '*' || c == '/';
    }
    static bool karakterValid(char c) {
        return isHuruf(c) || isOp(c) || c == '(' || c == ')';
    }
    static InputKind klasifikasi(char c) {
        if (isHuruf(c)) return HURUF;
        if (isOp(c))    return OP;
        if (c == '(')   return KBUKA;
        return KTUTUP;
    }

    // ----- Hasil pemeriksaan sebuah string -----
    struct Hasil {
        bool        diterima;
        std::string jenis;    // PREFIX / INFIX / POSTFIX / OPERAND TUNGGAL / "-"
        std::string jejak;    // jalur state penerimaan (jika diterima)
        std::string alasan;   // penjelasan jika ditolak
    };

    // -----------------------------------------------------------------
    //  Jalankan automata terhadap string (spasi & tab diabaikan)
    // -----------------------------------------------------------------
    Hasil jalankan(const std::string& input) const {
        Hasil hasil;
        hasil.diterima = false;
        hasil.jenis    = "-";
        hasil.jejak    = "";
        hasil.alasan   = "";

        // Saring token: buang spasi, validasi karakter
        std::string tokens;
        std::string simbolToken;            // karakter asli tiap token
        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
            if (!karakterValid(c)) {
                hasil.alasan = "Karakter tidak dikenal: '" + std::string(1, c) + "'";
                return hasil;
            }
            tokens += c;
        }
        simbolToken = tokens;               // 1 karakter = 1 token

        // --- Konfigurasi BFS: (state, posisi input, stack, jejak) ---
        struct Konfigurasi {
            int                state;
            size_t             pos;
            std::vector<char>  stack;
            std::vector<int>   jejak;       // indeks transisi yang dipakai
        };

        std::queue<Konfigurasi> antrian;
        Konfigurasi awal;
        awal.state = STATE_AWAL;
        awal.pos   = 0;
        awal.stack.push_back(PONDASI);      // inisialisasi stack: [R]
        antrian.push(awal);

        int  stateAkhirDiterima = -1;
        std::vector<int> jejakDiterima;

        while (!antrian.empty()) {
            Konfigurasi cfg = antrian.front();
            antrian.pop();

#ifdef DEBUG_PDA
            {
                printf("[BFS] pop state=%s pos=%zu/%zu stack=[", namaState(cfg.state), cfg.pos, tokens.size());
                for (size_t i = 0; i < cfg.stack.size(); ++i) printf("%c", cfg.stack[i]);
                printf("]\n");
            }
#endif

            // --- KONDISI PENERIMAAN: -----------------------------
            // berada di q6 (ACCEPT), seluruh input sudah dibaca,
            // dan stack hanya tersisa pondasi R.
            if (cfg.state == STATE_ACCEPT &&
                cfg.pos == tokens.size() &&
                cfg.stack.size() == 1 && cfg.stack[0] == PONDASI)
            {
                stateAkhirDiterima = cfg.state;
                jejakDiterima      = cfg.jejak;
                break;   // satu jalur penerimaan cukup
            }

            // --- Coba seluruh transisi yang berlaku --------------
            for (int t = 0; t < JUMLAH_TRANSISI; ++t) {
                const Transisi& tr = TABEL[t];
                if (tr.dari != cfg.state) continue;

                bool inputCocok =
                    (tr.input == EPS) ? (cfg.pos == tokens.size())
                                      : (cfg.pos <  tokens.size() &&
                                         klasifikasi(tokens[cfg.pos]) == tr.input);
                if (!inputCocok) continue;

                // Cek puncak stack harus sama dengan simbol pop
                if (cfg.stack.empty() || cfg.stack.back() != tr.pop) continue;

#ifdef DEBUG_PDA
                printf("[BFS]    apply T%d: %s -> %s (%s, %c -> %s)\n", t,
                       namaState(tr.dari), namaState(tr.ke), labelInput(tr.input),
                       tr.pop, tr.push[0] == '\0' ? "eps" : tr.push);
#endif

                // Terapkan: pindah state, pop simbol, lalu push string baru
                Konfigurasi lanjut = cfg;
                lanjut.state = tr.ke;
                lanjut.stack.pop_back();
                for (const char* p = tr.push; *p != '\0'; ++p)
                    lanjut.stack.push_back(*p);
                if (tr.input != EPS) ++lanjut.pos;   // input terkonsumsi
                lanjut.jejak.push_back(t);
                antrian.push(lanjut);
            }
        }

        // --- Susun hasil ------------------------------------------------
        if (stateAkhirDiterima >= 0) {
            hasil.diterima = true;
            int cabang = stateMasukAccept(jejakDiterima);
            hasil.jenis = (cabang == 1) ? "PREFIX"
                        : (cabang == 5) ? "POSTFIX"
                        : (cabang == 4) ? "INFIX"
                        : "OPERAND TUNGGAL";
            hasil.jejak = bentukJejak(jejakDiterima, simbolToken);
        } else {
            hasil.alasan = "Tidak ada jalur menuju state ACCEPT (q6); "
                           "string tidak dikenali sebagai ekspresi "
                           "Prefix/Infix/Postfix yang valid.";
        }
        return hasil;
    }

private:
    // State sebelum transisi terakhir (masuk ke q6) -> menentukan cabang
    static int stateMasukAccept(const std::vector<int>& jejak) {
        if (jejak.empty()) return -1;
        return TABEL[jejak.back()].dari;
    }

    // Bentuk string jalur: q0 --[huruf 'A', R->RA]--> q5 ...
    std::string bentukJejak(const std::vector<int>& jejak,
                            const std::string& simbolToken) const {
        std::ostringstream os;
        int state = STATE_AWAL;
        size_t pos = 0;
        os << namaState(state);
        for (size_t i = 0; i < jejak.size(); ++i) {
            const Transisi& tr = TABEL[jejak[i]];
            os << " --[" << labelInput(tr.input);
            if (tr.input != EPS && pos < simbolToken.size())
                os << " '" << simbolToken[pos] << "'";
            os << ", " << tr.pop << "->";
            os << (tr.push[0] == '\0' ? std::string("eps") : std::string(tr.push));
            os << "]--> " << namaState(tr.ke);
            if (tr.input != EPS) ++pos;
            state = tr.ke;
        }
        return os.str();
    }
};

// ---------------------------------------------------------------------
//  Definisi tabel transisi (lihat komentar header untuk peta lengkap)
// ---------------------------------------------------------------------
const char PushdownAutomata::PONDASI;
const char PushdownAutomata::KURUNG;
const char PushdownAutomata::OPERATOR;
const char PushdownAutomata::OPERAND;

const PushdownAutomata::Transisi PushdownAutomata::TABEL[] = {
    // ---- q0 : State Awal (percabangan sesuai token pertama) ----
    { 0, HURUF, 'R', "R",  2 },   // q0 -> q2 : huruf, R -> R
    { 0, HURUF, 'R', "RA", 5 },   // q0 -> q5 : huruf, R -> RA (postfix)
    { 0, OP,    'R', "RO", 1 },   // q0 -> q1 : op,    R -> RO (prefix)
    { 0, KBUKA, 'R', "RP", 3 },   // q0 -> q3 : (,     R -> RP (infix)

    // ---- q1 : Cabang Prefix ----
    { 1, OP,    'R', "RO", 1 },   // loop: op,    R -> RO
    { 1, OP,    'O', "OO", 1 },   // loop: op,    O -> OO
    { 1, HURUF, 'O', "",   1 },   // loop: huruf, O -> eps
    { 1, HURUF, 'R', "R",  6 },   // q1 -> q6 : huruf, R -> R (ACCEPT)

    // ---- q2 : Pendeteksi Awal ----
    { 2, OP,    'R', "R",  3 },   // q2 -> q3 : op, R -> R (masuk infix)
    { 2, EPS,   'R', "R",  6 },   // q2 -> q6 : eps, R -> R (operand tunggal)

    // ---- q3 : Menunggu Operand - Infix ----
    { 3, KBUKA, 'R', "RP", 3 },   // loop: (, R -> RP
    { 3, KBUKA, 'P', "PP", 3 },   // loop: (, P -> PP
    { 3, HURUF, 'R', "R",  4 },   // q3 -> q4 : huruf, R -> R
    { 3, HURUF, 'P', "P",  4 },   // q3 -> q4 : huruf, P -> P

    // ---- q4 : Setelah Operand - Infix ----
    { 4, KTUTUP,'P', "",   4 },   // loop: ), P -> eps
    { 4, OP,    'R', "R",  3 },   // q4 -> q3 : op, R -> R
    { 4, OP,    'P', "P",  3 },   // q4 -> q3 : op, P -> P
    { 4, EPS,   'R', "R",  6 },   // q4 -> q6 : eps, R -> R (ACCEPT)

    // ---- q5 : Cabang Postfix ----
    { 5, HURUF, 'A', "AA", 5 },   // loop: huruf, A -> AA
    { 5, OP,    'A', "",   5 },   // loop: op,    A -> eps
    { 5, EPS,   'A', "",   6 },   // q5 -> q6 : eps, A -> eps (ACCEPT)
};
const int PushdownAutomata::JUMLAH_TRANSISI =
    sizeof(PushdownAutomata::TABEL) / sizeof(PushdownAutomata::TABEL[0]);

// ---------------------------------------------------------------------
//  Tampilan bantuan
// ---------------------------------------------------------------------
static void cetakTabelTransisi() {
    std::cout << "TABEL TRANSISI PDA (input, pop -> push)\n";
    std::cout << "------------------------------------------------\n";
    for (int i = 0; i < PushdownAutomata::JUMLAH_TRANSISI; ++i) {
        const PushdownAutomata::Transisi& t = PushdownAutomata::TABEL[i];
        std::cout << "  "
                  << PushdownAutomata::namaState(t.dari) << " -> "
                  << PushdownAutomata::namaState(t.ke)   << " : "
                  << PushdownAutomata::labelInput(t.input) << ", "
                  << t.pop << " -> "
                  << (t.push[0] == '\0' ? "eps" : t.push) << "\n";
    }
    std::cout << "------------------------------------------------\n";
    std::cout << "Penerimaan: state q6 (ACCEPT), input habis, stack = [R]\n";
}

static void uji(const std::string& s) {
    PushdownAutomata pda;
    PushdownAutomata::Hasil h = pda.jalankan(s);

    std::cout << "--------------------------------------------------\n";
    std::cout << "Input  : \"" << s << "\"\n";
    std::cout << "Hasil  : "
              << (h.diterima ? "DITERIMA (ACCEPT)" : "DITOLAK (REJECT)")
              << "\n";
    if (h.diterima) {
        std::cout << "Notasi : " << h.jenis << "\n";
        std::cout << "Jalur  : " << h.jejak << "\n";
    } else {
        std::cout << "Alasan : " << h.alasan << "\n";
    }
}

// PDA_NO_MAIN: untuk meng-compile pda.cpp sebagai pustaka pengujian
#ifndef PDA_NO_MAIN
int main(int argc, char* argv[]) {
    // --tabel : cetak tabel transisi lalu keluar
    if (argc > 1 && std::string(argv[1]) == "--tabel") {
        cetakTabelTransisi();
        return 0;
    }

    std::cout << "====================================================\n";
    std::cout << "  PDA - PENGENAL NOTASI PREFIX, INFIX & POSTFIX\n";
    std::cout << "  (implementasi diagram pushdown automata)\n";
    std::cout << "====================================================\n";
    std::cout << "Keterangan input:\n";
    std::cout << "  huruf/operand : A-Z, a-z, 0-9\n";
    std::cout << "  operator (op) : + - * /\n";
    std::cout << "  kurung        : ( )\n";
    std::cout << "  perintah      : 'tabel' = lihat tabel transisi,\n";
    std::cout << "                  'exit'  = keluar\n\n";

    // Argumen baris perintah: uji string tsb langsung
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) uji(argv[i]);
        return 0;
    }

    // Contoh pengujian otomatis
    std::cout << "========== CONTOH PENGUJIAN OTOMATIS ==========\n";
    std::cout << "-- PREFIX --\n";
    uji("+ A B");            // valid   : +AB
    uji("- * A B C");        // valid   : (A*B)-C
    uji("+ - A B * C D");    // valid   : (A-B)+(C*D)
    uji("+ A");              // invalid : operand kurang
    uji("+ A B C");          // invalid : operand lebih
    std::cout << "-- INFIX --\n";
    uji("A + B");            // valid
    uji("( A + B ) * C");    // valid berkurung
    uji("A + B * C - D");    // valid
    uji("( ( A + B ) )");    // valid kurung bertingkat
    uji("A + ");             // invalid : operator menggantung
    uji("( A + B");          // invalid : kurung tidak ditutup
    uji("A + + B");          // invalid : operator ganda
    std::cout << "-- POSTFIX --\n";
    uji("A B +");            // valid   : AB+
    uji("A B * C -");        // valid   : (A*B)-C
    uji("A B C * + D -");    // valid   : A-(D+B*C)... (A+B*C)-D
    uji("A B + C");          // invalid : operand lebih
    uji("A B + +");          // invalid : operator lebih
    std::cout << "-- LAIN-LAIN --\n";
    uji("A");                // operand tunggal
    uji("A B");              // invalid
    std::cout << "========== AKHIR CONTOH ==========\n\n";

    // Mode interaktif
    std::string baris;
    while (true) {
        std::cout << "\nEkspresi> ";
        if (!std::getline(std::cin, baris)) break;
        if (baris == "exit" || baris == "quit") break;
        if (baris == "tabel") { cetakTabelTransisi(); continue; }
        if (baris.empty()) continue;
        uji(baris);
    }

    std::cout << "Selesai.\n";
    return 0;
}
#endif  // PDA_NO_MAIN
