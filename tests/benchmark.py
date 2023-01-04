# Copyright (c) 2023 Gleb Koveshnikov

import os
import sys
import time

import tensorflow

from tensorflow_text import BertTokenizer as TensorflowBertTokenizer
from tokenizers import BertWordPieceTokenizer as HuggingFaceBertTokenizer
from torchtext.transforms import BERTTokenizer as TorchBertTokenizer


HUGGING_FACE = 'hugging face'
NAIVE = 'naive'
TENSORFLOW = 'tensorflow'
TORCH = 'torch'
WORD_PIECE = 'word piece'

ALGORITHMS = [TENSORFLOW, TORCH, WORD_PIECE, NAIVE, HUGGING_FACE]

# TODO: check
# https://github.com/pytorch/text/blob/8eb056103cd1d518d53252dd63d3c75f284345ca/benchmark/benchmark_bert_tokenizer.py
# https://github.com/pytorch/text/blob/5eb33ce1f7447df5069b6cfb55b8177c9bbaff08/test/torchtext_unittest/test_transforms.py
# https://huggingface.co/transformers/v3.0.2/pretrained_models.html
# https://github.com/VKCOM/YouTokenToMe/pull/101/files#

def run_tensorflow(text_file, vocab_file):
    text = ""
    with open(text_file, 'r') as f:
        text = f.read()
    vocab_list = []
    with open(vocab_file, 'r') as f:
        for word in f:
            vocab_list.append(word)
    lookup_table = tensorflow.lookup.StaticVocabularyTable(
        tensorflow.lookup.KeyValueTensorInitializer(
            keys=vocab_list,
            key_dtype=tensorflow.string,
            values=tensorflow.range(
                tensorflow.size(vocab_list, out_type=tensorflow.int64), dtype=tensorflow.int64),
            value_dtype=tensorflow.int64
        ),
        num_oov_buckets=1
    )
    bert_tokenizer = TensorflowBertTokenizer(lookup_table, token_out_type=tensorflow.int64)
    ids = bert_tokenizer.tokenize(text).merge_dims(1, -1)
    return ids


def run_hugging_face(text_file, vocab_file):
    with open(text_file, 'r') as f:
        text = f.read()
    tokenizer = HuggingFaceBertTokenizer(vocab_file)
    print(tokenizer)
    ids = tokenizer.encode(text).ids
    assert len(ids) > 0
    return ids


def run_torch(text_file, vocab_file):
    with open(text_file, 'r') as f:
        text = f.read()
    tokenizer = TorchBertTokenizer(vocab_file)
    ids = tokenizer(text)
    assert len(ids) > 0
    return ids


def run_word_piece(text_file, vocab_file):
    rc = os.system(f"./build/tests/runner real {text_file} {vocab_file}")
    print(f'{WORD_PIECE} returned {rc}')
    assert rc == 0
    return rc

def run_naive(text_file, vocab_file):
    rc = os.system(f"./build/tests/runner naive {text_file} {vocab_file}")
    print(f'{NAIVE} returned {rc}')
    assert rc == 0
    return rc


def run_algorithm(algorithm, text_file, vocab_file):
    algorithm_map = {
        HUGGING_FACE: run_hugging_face,
        NAIVE: run_naive,
        TORCH: run_torch,
        TENSORFLOW: run_tensorflow,
        WORD_PIECE: run_word_piece,
    }

    algorithm_func = algorithm_map[algorithm]
    return algorithm_func(text_file, vocab_file)


def run_benchmark(text_file, vocab_file):
    result = []
    for algorithm in ALGORITHMS:
        before = time.time_ns()
        print(f'Running {algorithm}')
        res = run_algorithm(algorithm, text_file, vocab_file)
        duration_ms = (time.time_ns() - before) // 1_000_000
        print(f'{algorithm} finished in {duration_ms} ms')
        result.append((duration_ms, algorithm))

    result = sorted(result)
    return result


def cut(source_file, destination_file, text_size_mb):
    text_size_bytes = text_size_mb * 1_000_000
    processed_bytes = 0
    with open(source_file, "r") as fin:
        with open(destination_file, "w") as fout:
            while processed_bytes < text_size_bytes:
                line = fin.readline()
                processed_bytes += len(line.encode())
                fout.write(line)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: benchmark.py <DATA_FILE> <VOCAB_FILE> <TEXT_SIZE_MB>.")
        raise ValueError

    data_file = str(sys.argv[1])
    vocab_file = str(sys.argv[2])
    text_size_mb = int(sys.argv[3])

    if text_size_mb != -1:
        text_file = data_file + f'{text_size_mb}.data'
        remove_text_file = True
        cut(data_file, text_file, text_size_mb)
    else:
        text_file = data_file
        remove_text_file = False

    result = run_benchmark(text_file, vocab_file)
    if remove_text_file:
        os.remove(text_file)

    print("==================================================")
    print("Benchmark is finished.")
    for algo in result:
        duration_sec = algo[0] / 1000
        print(f'{algo[1]}: {duration_sec:.1f} sec')
