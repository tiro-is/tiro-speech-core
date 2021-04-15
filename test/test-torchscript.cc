#include <torch/script.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

int main(int argc, const char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: test-torchscript <path-to-exported-script-module>\n";
    return -1;
  }

  torch::jit::Module module;
  try {
    module = torch::jit::load(
        std::filesystem::path{std::getenv("TEST_SRCDIR")} / argv[1]);
    module.eval();
  } catch (const c10::Error& e) {
    std::cerr << "error loading the model\n";
    std::cerr << e.what();
    return -1;
  }

  torch::Tensor input_ids = torch::tensor(
      {{2,    4183,  273,   4466,  1885, 5664,  1801,  1744, 3144, 17336,
        261,  16326, 1843,  1730,  3719, 2082,  11206, 2147, 2077, 1768,
        2715, 1730,  2126,  2460,  4712, 12092, 1752,  1744, 2368, 2147,
        3004, 1851,  9429,  1735,  2460, 5185,  9957,  1735, 1990, 1744,
        7540, 1871,  9947,  2342,  1730, 1801,  2098,  2368, 273,  13593,
        3934, 2027,  2234,  1840,  5670, 1768,  1885,  1968, 261,  15700,
        3388, 1142,  15657, 15700, 3388, 1142,  20989, 1729, 2765, 0}});

  // Hardcode these for now
  std::array<std::string, 4> id2label{"O", "COMMA", "PERIOD", "QUESTIONMARK"};
  std::array<std::string, 4> id2char{"", ",", ".", "?"};

  std::cout << "size" << input_ids.sizes() << '\n';

  auto output = module.forward({input_ids}).toTuple();
  std::cout << output->elements()[0] << '\n';

  torch::Tensor pred_ids =
      torch::argmax(output->elements()[0].toTensor(), 2, true).flatten();

  std::cout << "predictions:\n" << pred_ids << '\n';
}

// Example input:
//
// ['eftirlit', 'í', 'gangi', 'ég', 'segi', 'það', 'er', 'lögreglu', '##liðin',
// 'á', 'tá', '##num', 'að', 'sinna', 'þessu', 'eftirliti', 'mjög', 'vel',
// 'við', 'erum', 'að', 'fá', 'líka', 'margar', 'ábendingar', 'sem', 'er',
// 'bara', 'mjög', 'gott', 'frá', 'almenningi', 'og', 'líka', 'rekstrar',
// '##aðilum', 'og', 'þeim', 'er', 'fylgt', 'eftir', 'ýmsir', 'segja', 'að',
// 'það', 'væri', 'bara', 'í', 'ágætis', 'málum', 'hjá', 'okkur', 'hann',
// 'bætir', 'við', 'ég', 'inn', 'á', 'co', '##vi', '##d', 'síðunni', 'co',
// '##vi', '##d', 'punkt', '##ur', 'bis']
// tensor([[    2,  4183,   273,  4466,  1885,  5664,  1801,  1744,  3144,
// 17336,
//            261, 16326,  1843,  1730,  3719,  2082, 11206,  2147,  2077, 1768,
//           2715,  1730,  2126,  2460,  4712, 12092,  1752,  1744,  2368, 2147,
//           3004,  1851,  9429,  1735,  2460,  5185,  9957,  1735,  1990, 1744,
//           7540,  1871,  9947,  2342,  1730,  1801,  2098,  2368,   273,
//           13593, 3934,  2027,  2234,  1840,  5670,  1768,  1885,  1968, 261,
//           15700, 3388,  1142, 15657, 15700,  3388,  1142, 20989,  1729, 2765,
//           0]])
