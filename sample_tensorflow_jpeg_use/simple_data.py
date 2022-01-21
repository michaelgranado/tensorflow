import matplotlib.pyplot as plt
import numpy as np
import os
import PIL
import tensorflow as tf

from tensorflow import keras
from tensorflow.keras import layers
from tensorflow.keras.models import Sequential

import pathlib
dataset_url = "https://storage.googleapis.com/download.tensorflow.org/example_images/flower_photos.tgz"
data_dir = tf.keras.utils.get_file('flower_photos', origin=dataset_url, untar=True)
data_dir = pathlib.Path(data_dir)

image_count = len(list(data_dir.glob('*/*.jpg')))

roses_count = len(list(data_dir.glob('roses/*')))

dandelions_count = len(list(data_dir.glob('dandelion/*')))

daisies_count = len(list(data_dir.glob('daisy/*')))

sunflowers_count = len(list(data_dir.glob('sunflowers/*')))

tulips_count = len(list(data_dir.glob('tulips/*')))

print(roses_count,"roses")
print(tulips_count,"tulips")
print(dandelions_count,"dandelions")
print(daisies_count,"daisies")
print(sunflowers_count,"sunflowers")
print("-------------------------")
print(image_count,"total images")


batch_size = 32
img_height = 180
img_width = 180

train_ds = tf.keras.utils.image_dataset_from_directory(
  data_dir,
  validation_split=0.2,
  subset="training",
  seed=123,
  image_size=(img_height, img_width),
  batch_size=batch_size)
print("-------------------------")

val_ds = tf.keras.utils.image_dataset_from_directory(
  data_dir,
  validation_split=0.2,
  subset="validation",
  seed=123,
  image_size=(img_height, img_width),
  batch_size=batch_size)
print("-------------------------")

class_names = train_ds.class_names

print("image_batch and label_batch shapes")

for image_batch, labels_batch in train_ds:
  print(image_batch.shape)
  print(labels_batch.shape)
  break
