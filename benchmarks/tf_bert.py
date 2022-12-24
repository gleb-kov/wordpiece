import os
import time

import tensorflow

from tensorflow_text import BertTokenizer as TensorflowBertTokenizer
from tokenizers import BertWordPieceTokenizer as HuggingFaceBertTokenizer
from torchtext.transforms import BERTTokenizer as TorchBertTokenizer

# TODO: check https://github.com/pytorch/text/blob/8eb056103cd1d518d53252dd63d3c75f284345ca/benchmark/benchmark_bert_tokenizer.py

def run_tensorflow(text, vocab_file):
    vocab_list = []
    with open(vocab_file, 'r') as f:
        for word in f:
            vocab_list.append(word)
    lookup_table = tf.lookup.StaticVocabularyTable(
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
    return bert_tokenizer.tokenize(text).merge_dims(1, -1)


def run_hugging_face(text, vocab_file):
    tokenizer = HuggingFaceBertTokenizer(vocab_file)
    print(tokenizer)
    output = tokenizer.encode(text)
    return output.ids


def run_torch(text, vocab_file):
    tokenizer = TorchBertTokenizer(vocab_file)
    return tokenizer(text)


def run_fastest_naive(text, vocab_file):
    pass


def run_benchmark(impls, text, vocab_file):
    before = time.time_ns()
    impl(text, vocab_list)
    duration = time.time_ns() - before
    print(duration)


if __name__ == "__main__":
    text = "Sponge bob Squarepants is an Avenger Marvel Avengers"
    vocab_list = [
        "##ack", "##ama", "##ger", "##gers", "##onge", "##pants", "##uare",
        "##vel", "##ven", "an", "A", "Bar", "Hates", "Mar", "Ob",
        "Patrick", "President", "Sp", "Sq", "bob", "box", "has", "highest",
        "is", "office", "the",
    ]
    vocab_file = 'bert_vocab.txt'
    with open(vocab_filename, 'w') as f:
        for word in vocab_list:
            f.write(f'{word}\n')

    for impl in [run_hugging_face, run_tensorflow, run_torch, run_fastest_naive]:
        run_benchmark(impl, text, vocab_file)
