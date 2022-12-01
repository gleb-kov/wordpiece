import tensorflow as tf
import tensorflow_text as tf_text
import time


def process(text, vocab_list):
    lookup_table = tf.lookup.StaticVocabularyTable(
        tf.lookup.KeyValueTensorInitializer(
            keys=vocab_list,
            key_dtype=tf.string,
            values=tf.range(
                tf.size(vocab_list, out_type=tf.int64), dtype=tf.int64),
            value_dtype=tf.int64
        ),
        num_oov_buckets=1
    )
    bert_tokenizer = tf_text.BertTokenizer(lookup_table, token_out_type=tf.int64)
    return bert_tokenizer.tokenize(text).merge_dims(1, -1)


if __name__ == "__main__":
    text = "Sponge bob Squarepants is an Avenger Marvel Avengers"
    vocab_list = [
        "##ack", "##ama", "##ger", "##gers", "##onge", "##pants", "##uare",
        "##vel", "##ven", "an", "A", "Bar", "Hates", "Mar", "Ob",
        "Patrick", "President", "Sp", "Sq", "bob", "box", "has", "highest",
        "is", "office", "the",
    ]

    before = time.time_ns()
    process(text, vocab_list)
    duration = time.time_ns() - before
    print(duration)
