import os
import time

import tensorflow

from tensorflow_text import BertTokenizer as TensorflowBertTokenizer
from tokenizers import BertWordPieceTokenizer as HuggingFaceBertTokenizer
from torchtext.transforms import BERTTokenizer as TorchBertTokenizer

TEXT_FILE = "text.txt"
VOCAB_FILE = "vocab.txt"

TENSORFLOW = 'tensorflow'
HUGGING_FACE = 'hugging face'
TORCH = 'torch'
NAIVE = 'naive'
WORD_PIECE = 'wordpiece'

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
    rc = os.system(f"./runner real {text_file} {vocab_file}")
    assert rc == 0
    return rc

def run_fast_naive(text_file, vocab_file):
    rc = os.system(f"./runner naive {text_file} {vocab_file}")
    assert rc == 0
    return rc


def run_algo(algo_name, text_file, vocab_file):
    algo = None
    if algo_name == TENSORFLOW:
        algo = run_tensorflow
    elif algo_name == HUGGING_FACE:
        algo = run_hugging_face
    elif algo_name == TORCH:
        algo = run_torch
    elif algo_name == NAIVE:
        algo = run_naive
    elif algo_name == WORD_PIECE:
        algo = run_word_piece

    return algo(text_file, vocab_file)


if __name__ == "__main__":
    result = []
    for algo_name in [HUGGING_FACE, TORCH, TENSORFLOW]:
        before = time.time_ns()
        res = run_algo(algo_name, TEXT_FILE, VOCAB_FILE)
        duration = time.time_ns() - before
        result.append((duration, algo_name))
    result = sorted(result)

    print("==================================================")
    print("Benchmark is finished.")
    for algo in result:
        print(f'{algo[1]}: {algo[0] // 1000} mcs')
