import argparse
import logging

import numpy as np
import pandas as pd
import torch
import torch.nn as nn
from pytorch_lightning import Trainer
from pytorch_lightning.callbacks import ModelCheckpoint
from pytorch_lightning.core.lightning import LightningModule
from torch.utils.data import DataLoader, Dataset
from transformers.optimization import AdamW, get_cosine_schedule_with_warmup
from transformers import PreTrainedTokenizerFast, GPT2LMHeadModel
from transformers import AutoTokenizer
from itertools import islice
import csv
import random
import ast

EmoClassifierDefaultConfig = {
    'num_layers': 2,
    'hidden_dim' : 768,
    'n_vocab' : 51200,
    'embed_dim': 768,
    'num_classes' : 6,
    'dropout': 0.1,
    'bidirectional': True
}


PATH_PRE = '/content/drive/Shareddrives/종합설계프로젝트 2조/Jiyoun/Chatbot_Pretrain'
PATH_EMO = '/content/drive/Shareddrives/종합설계프로젝트 2조/ckpt'
U_TKN = '<usr>'
S_TKN = '<sys>'
BOS = '</s>'
EOS = '</s>'
MASK = '<unused0>'
SENT = '<unused1>'
PAD = '<pad>'

TOKENIZER = PreTrainedTokenizerFast.from_pretrained("skt/kogpt2-base-v2",
            bos_token=BOS, eos_token=EOS, unk_token='<unk>',
            pad_token=PAD, mask_token=MASK) 


class CharDataset(Dataset):
    def __init__(self, chats, max_len=32):
        self._data = chats
        self.first = True
        self.q_token = U_TKN
        self.a_token = S_TKN
        self.sent_token = SENT
        self.bos = BOS
        self.eos = EOS
        self.mask = MASK
        self.pad = PAD
        self.max_len = max_len
        self.tokenizer = TOKENIZER 

    def __len__(self):
        return len(self._data)

    def __getitem__(self, idx):
        turn = self._data.iloc[idx]
        q = turn['Q']
        a = turn['A']
        sentiment = str(turn['label'])
        q_toked = self.tokenizer.tokenize(self.q_token + q + \
                                          self.sent_token + sentiment)   
        q_len = len(q_toked)
        a_toked = self.tokenizer.tokenize(self.a_token + a + self.eos)
        a_len = len(a_toked)
        if q_len + a_len > self.max_len:
            a_len = self.max_len - q_len
            if a_len <= 0:
                q_toked = q_toked[-(int(self.max_len/2)):]
                q_len = len(q_toked)
                a_len = self.max_len - q_len
                assert a_len > 0
            a_toked = a_toked[:a_len]
            a_len = len(a_toked)
            assert a_len == len(a_toked), f'{a_len} ==? {len(a_toked)}'
        # [mask, mask, ...., mask, ..., <bos>,..A.. <eos>, <pad>....]
        labels = [
            self.mask,
        ] * q_len + a_toked[1:]
        if self.first:
            logging.info("contexts : {}".format(q))
            logging.info("toked ctx: {}".format(q_toked))
            logging.info("response : {}".format(a))
            logging.info("toked response : {}".format(a_toked))
            logging.info('labels {}'.format(labels))
            self.first = False
        mask = [0] * q_len + [1] * a_len + [0] * (self.max_len - q_len - a_len)
        self.max_len
        labels_ids = self.tokenizer.convert_tokens_to_ids(labels)
        while len(labels_ids) < self.max_len:
            labels_ids += [self.tokenizer.pad_token_id]
        token_ids = self.tokenizer.convert_tokens_to_ids(q_toked + a_toked)
        while len(token_ids) < self.max_len:
            token_ids += [self.tokenizer.pad_token_id]
        return(token_ids, np.array(mask),
               labels_ids)


class KoGPT2Chat(LightningModule):
    def __init__(self, hparams, **kwargs):
        super(KoGPT2Chat, self).__init__()
        # breakpoint()
        # self.hparams = hparams
        self.save_hyperparameters()
        for key in hparams.keys():
            self.hparams[key] = hparams[key]
        self.neg = -1e18
        self.kogpt2 = GPT2LMHeadModel.from_pretrained('skt/kogpt2-base-v2')
        self.loss_function = torch.nn.CrossEntropyLoss(reduction='none')

    @staticmethod
    def add_model_specific_args(parent_parser):
        # add model specific args
        parser = argparse.ArgumentParser(parents=[parent_parser], add_help=False)
        parser.add_argument('--max-len',
                            type=int,
                            default=32,
                            help='max sentence length on input (default: 32)')

        parser.add_argument('--batch-size',
                            type=int,
                            default=192,
                            help='batch size for training (default: 96)')
        parser.add_argument('--lr',
                            type=float,
                            default=5e-5,
                            help='The initial learning rate')
        parser.add_argument('--warmup_ratio',
                            type=float,
                            default=0.1,
                            help='warmup ratio')
        return parser

    def forward(self, inputs):
        # (batch, seq_len, hiddens)
        # breakpoint()
        output = self.kogpt2(inputs, return_dict=True)
        # breakpoint()
        return output.logits

    def training_step(self, batch, batch_idx):
        token_ids, mask, label = batch
        # breakpoint()
        out = self(token_ids)
        # breakpoint()
        mask_3d = mask.unsqueeze(dim=2).repeat_interleave(repeats=out.shape[2], dim=2)
        # breakpoint()
        mask_out = torch.where(mask_3d == 1, out, self.neg * torch.ones_like(out))
        # breakpoint()
        loss = self.loss_function(mask_out.transpose(2, 1), label)
        # breakpoint()
        loss_avg = loss.sum() / mask.sum()
        # breakpoint()
        self.log('train_loss', loss_avg)
        return loss_avg

    def configure_optimizers(self):
        # Prepare optimizer
        param_optimizer = list(self.named_parameters())
        no_decay = ['bias', 'LayerNorm.bias', 'LayerNorm.weight']
        optimizer_grouped_parameters = [
            {'params': [p for n, p in param_optimizer if not any(nd in n for nd in no_decay)], 'weight_decay': 0.01},
            {'params': [p for n, p in param_optimizer if any(nd in n for nd in no_decay)], 'weight_decay': 0.0}
        ]
        # breakpoint()
        optimizer = AdamW(optimizer_grouped_parameters,
                          lr=self.hparams.lr, correct_bias=False)

        # breakpoint()
        # warm up lr
        num_train_steps = len(self.train_dataloader()) * self.hparams.max_epochs
        num_warmup_steps = int(num_train_steps * self.hparams.warmup_ratio)
        scheduler = get_cosine_schedule_with_warmup(
            optimizer,
            num_warmup_steps=num_warmup_steps, num_training_steps=num_train_steps)
        lr_scheduler = {'scheduler': scheduler, 'name': 'cosine_schedule_with_warmup',
                        'monitor': 'loss', 'interval': 'step',
                        'frequency': 1}
        return [optimizer], [lr_scheduler]

    def _collate_fn(self, batch):
        data = [item[0] for item in batch]
        mask = [item[1] for item in batch]
        label = [item[2] for item in batch]
        return torch.LongTensor(data), torch.LongTensor(mask), torch.LongTensor(label)

    def train_dataloader(self):
        data = pd.read_csv(PATH_PRE +'/Chatbot_Pretrain_Jiyoun/Chatbot_data/ChatbotData.csv')
        self.train_set = CharDataset(data, max_len=self.hparams.max_len)
        train_dataloader = DataLoader(
            self.train_set, batch_size=self.hparams.batch_size, num_workers=2,
            shuffle=True, collate_fn=self._collate_fn)
        return train_dataloader

    def chat(self, msg, sent='0'):
        tok = TOKENIZER
        sent_tokens = tok.tokenize(sent)
        with torch.no_grad():
            q = msg.strip()
            a = ''
            while 1:
                input_ids = torch.LongTensor(tok.encode(U_TKN + q + SENT + sent + S_TKN + a)).unsqueeze(dim=0)
                pred = self(input_ids)
                gen = tok.convert_ids_to_tokens(
                    torch.argmax(
                        pred,
                        dim=-1).squeeze().numpy().tolist())[-1]
                if gen == EOS:
                    break
                a += gen.replace('▁', ' ')
            return (a.strip())


class EmoClassifier(nn.Module):
    def __init__(self, num_layers, hidden_dim, n_vocab, embed_dim, num_classes, dropout, bidirectional):
        super(EmoClassifier, self).__init__()
        self.num_layers = num_layers
        self.hidden_dim = hidden_dim
        self.embed = nn.Embedding(n_vocab, embed_dim)
        self.gru = nn.GRU(embed_dim, self.hidden_dim,
                          num_layers=self.num_layers,
                          batch_first=True)
        self.dropout = nn.Dropout(p=dropout)
        self.out = nn.Linear(self.hidden_dim, num_classes)
        
    def forward(self, x): 
        """
        Args:
            input_embed: (N, seq_len, emb_dim)
            seq_len: (N) (length of each sequence, excluding padded part)

        Return:
            logits: (N, C(6))
            Emotion logits
        """
        x = self.embed(x)
        h_0 = self._init_state(batch_size=x.size(0))
        x, _ = self.gru(x, h_0)
        h_t = x[:,-1,:]
        self.dropout(h_t)
        logit = self.out(h_t) 

        return logit

    def _init_state(self, batch_size=16):
        weight = next(self.parameters()).data

        return weight.new(self.num_layers, batch_size, self.hidden_dim).zero_()


class MusicRecommendLogic(nn.Module):
    def __init__(
        self,
        EmoClassifierDefaultConfig
    ):
        super().__init__()
        # emotion classifier 모델이 사용한 tokenizer
        self.tokenizer = AutoTokenizer.from_pretrained('skt/kogpt2-base-v2',
                bos_token=BOS, eos_token=EOS, unk_token='<unk>',
                pad_token=PAD, mask_token='<mask>')
        self.emo_classifier = EmoClassifier(**EmoClassifierDefaultConfig)
        # emotion classifier ckpt
        checkpoint = torch.load(PATH_EMO+'/checkpoint_epoch_4.pt')
        # emotion classifier ckpt 로 파라미터 초기화
        self.emo_classifier.load_state_dict(checkpoint['model_state_dict'])
        # self.emo_classifier.to(device)
        self.emo_classifier.eval()

    def forward(self, user_chat_hist, emotion_list):
      input_ids = torch.LongTensor(self.tokenizer.encode(BOS + user_chat_hist + EOS)).unsqueeze(dim=0)
      # input_ids = input_ids.to(device)
      # bos token 의 hidden 벡터 softmax 값으로 sentiment classification
      pred = self.emo_classifier(input_ids)
      sentiment = torch.nn.functional.softmax(torch.FloatTensor(pred[0].cpu())).tolist()
      max_sentiment_idx = max(range(len(sentiment)), key=sentiment.__getitem__)

      # (동준님 순서) sentiment order: [기쁨, 슬픔, 불안, 분노, 상처, 당황] -> 이 순서로 가기로 함.
      # (지현님 순서) emotion_list order: [슬픔, 화남, 불안, 상처, 당황, 기쁨]

      genre = emotion_list[max_sentiment_idx]

      if genre == 0: # 발라드
        genre_code = 'GN2505'
      elif genre == 1: # 댄스
        genre_code = 'GN2506'
      elif genre == 2: # 랩/힙합
        genre_code = 'GN0300'
      elif genre == 3: # 록/메탈
        genre_code = 'GN0600'
      elif genre == 4: # R&B/Soul
        genre_code = 'GN0400'
      elif genre == 5: # 인디음악
        genre_code = 'GN0500'
      elif genre == 6: # 팝
        genre_code = 'GN0900'
      elif genre == 7: # 클래식
        genre_code = 'GN1600'
      elif genre == 8: # 재즈
        genre_code = 'GN1700'

      # TODO [after df update] need row count change 
      random_start = random.randint(0, (54280 - 2000))

      recommend_string = ""
      
      # TODO [after df update] need df path change
      with open(PATH_EMO+'/music_list.csv') as csv_file:
        csv_reader = csv.reader(csv_file, delimiter='\t')
        num_songs = 0
        for i_row, row in enumerate(islice(csv_reader,  random_start, random_start + 2001)):
          genre_list1 = ast.literal_eval(row[5])
          genre_list2 = ast.literal_eval(row[6])
          genre_list = genre_list1 + genre_list2
          if genre_code in genre_list: # 제목///아티스트명///앨범명
            recommend_string += row[1]
            recommend_string += "///"
            artist_list = ast.literal_eval(row[2])
            len_artist = len(artist_list)
            for i_a, a in enumerate(artist_list):
              recommend_string += a
              if i_a is not len_artist - 1:
                recommend_string += ", "
            recommend_string += "///"
            recommend_string += row[4]
            num_songs += 1
            recommend_string += "///"
          if num_songs == 10:
            break
      
      return recommend_string


if __name__ == "__main__":
    my_args = {'chat': True, 'sentiment': '0', 'model_params': 'model_chp/model_-last.ckpt', 'train': False, 'max_len': 32, 'batch_size': 192, 'lr': 5e-05, 'warmup_ratio': 0.1, 'logger': True, 'checkpoint_callback': None, 'enable_checkpointing': True, 'default_root_dir': None, 'gradient_clip_val': None, 'gradient_clip_algorithm': None, 'process_position': 0, 'num_nodes': 1, 'num_processes': None, 'devices': None, 'gpus': 0, 'auto_select_gpus': False, 'tpu_cores': None, 'ipus': None, 'log_gpu_memory': None, 'progress_bar_refresh_rate': None, 'enable_progress_bar': True, 'overfit_batches': 0.0, 'track_grad_norm': -1, 'check_val_every_n_epoch': 1, 'fast_dev_run': False, 'accumulate_grad_batches': None, 'max_epochs': None, 'min_epochs': None, 'max_steps': -1, 'min_steps': None, 'max_time': None, 'limit_train_batches': None, 'limit_val_batches': None, 'limit_test_batches': None, 'limit_predict_batches': None, 'val_check_interval': None, 'flush_logs_every_n_steps': None, 'log_every_n_steps': 50, 'accelerator': None, 'strategy': None, 'sync_batchnorm': False, 'precision': 32, 'enable_model_summary': True, 'weights_summary': 'top', 'weights_save_path': None, 'num_sanity_val_steps': 2, 'resume_from_checkpoint': None, 'profiler': None, 'benchmark': None, 'deterministic': False, 'reload_dataloaders_every_n_epochs': 0, 'auto_lr_find': False, 'replace_sampler_ddp': True, 'detect_anomaly': False, 'auto_scale_batch_size': False, 'prepare_data_per_node': None, 'plugins': None, 'amp_backend': 'native', 'amp_level': None, 'move_metrics_to_cpu': False, 'multiple_trainloader_mode': 'max_size_cycle', 'stochastic_weight_avg': False, 'terminate_on_nan': None}
    model = KoGPT2Chat(my_args)
    checkpoint = torch.load(PATH_PRE+'/Chatbot_Pretrain_Jiyoun/lightning_logs/version_2/checkpoints/epoch=2-step=186.ckpt')
    model.load_state_dict(checkpoint["state_dict"])
    #model.chat()

    # 1. emotion classifier 모델에 checkpoint 로드해서 모델 초기화
    music_recommend = MusicRecommendLogic(EmoClassifierDefaultConfig)
    # 2. user_chat_hist(string): 사용자 input text history, emotion_list(list): 감정-장르 list 로 데이터 넘겨주면 됩니다.
    # recommend_string = music_recommend(user_chat_hist, emotion_list)
    
    
from socket import *
import time
import datetime
import select
import pymysql
import queue
import threading



def JhPoll(sck, wait_time, time_link_check=0) :
  READ_ONLY = select.POLLIN | select.POLLHUP | select.POLLERR

  poller=select.poll()
  poller.register(sck,READ_ONLY)
  sfd = sck.fileno()
  cur_time = time.time()

  while True:
      if(time_link_check != 0 and cur_time - time_link_check > 70): return -1
      events = poller.poll(wait_time) 
   
      for fd,flag in events: #    (fileno,falg)   
          if fd is sfd:
              if flag & (select.POLLIN): #      elif,  fd       
                  return 1
              if flag & (select.POLLHUP | select.POLLERR):
                  return -1
            
      return 0



def ReadChunkData(sck, length, wait_sec, check_link_time = 0):
  timeOut = time.time() + wait_sec
  size = 0
  buff = ""

  while(time.time() <= timeOut and size < length):
    wtm = (timeOut - time.time())*1000
    check = JhPoll(sck, wtm, check_link_time)
    if(check == 0): continue
    elif (check < 0): return -1, buff
    else:
      curLen = length - size
      temp = sck.recv(curLen)

      if(len(temp) == 0): 
        print("1: size:", size, " length:",length, " curLen:", len(temp)," data: ", temp)
        return -1, buff
      
      size += len(temp)

      #print("2: size:", size, " length:",length, " curLen:", len(temp)," data: ", temp)

      buff = buff.join(temp.decode('utf-8'))
      

  return size, buff


def SendChunkData(sck, buff, length, wait_sec) :
  timeOut = time.time() + wait_sec
  size = 0
  buff = buff.encode('utf-8')

  while(time.time() <= timeOut and size < length):
    temp = buff[size:]
    wlen = sck.send(temp)
    size += wlen

  return size


class Worker(threading.Thread):
    def __init__(self, name):
        super().__init__()
        self.name = name            # thread 이름 지정
        self.conn = pymysql.connect(host='lbsg98.duckdns.org', port=20000, user='echatbot', password='echatbot2016', db='echatbotdb', charset='utf8')
        self.cursor = conn.cursor()


    def run(self):
        print("sub thread start ", self.name, "\n")
        while(True):
          if(q.qsize == 0):
            print("empty! sleep zzz")
            time.sleep(1)
          else:
            data = q.get()
            print("get", data)
            id=str(data['id'])
            msgR="<div><span>아이유</span><span>뭐해</span></div><div><span>아이유</span><span>뭐해</span></div><div><span>아이유</span><span>뭐해</span></div>"
            query = "UPDATE user SET music='"+msgR+"' where id="+id
            cursor.execute(query)
            print(msgR)
            conn.commit()
        print("sub thread end ", self.name, "\n")
        
def socketRun():
  while True:
    clientSock = socket(AF_INET, SOCK_STREAM)
    addr = "lbsg98.duckdns.org"
    #check_link_time = time.time()
    check_link_time = time.time()

    try:
      clientSock.connect((addr, 18083))
      check_link_time = time.time()
      now = datetime.datetime.now()
      print("연결되었습니다. ", now)

    except Exception as e:
      now = datetime.datetime.now()
      print("connect error : ",now, e)
      clientSock.close()
      time.sleep(3)
      continue


    while True:
        pollCount = JhPoll(clientSock, 1000, check_link_time)
        if(pollCount < 0):
          print("poll error")
          clientSock.close()
          break
        elif(pollCount == 0) : continue
        
        # read recieve packet head
        headLen, head = ReadChunkData(clientSock, 8, 15)
        if(headLen != 8):
          print("Head read error")
          clientSock.close()
          break
        #head = head.decode("utf-8")
        #print("head: ", head[:4])
        
        # read recieve packet data
        dataLen = int(head[:4])
        recvDataLen, recvData = ReadChunkData(clientSock, dataLen, 60)
        if(recvDataLen != dataLen):
          print("Data read error")
          clientSock.close()
          break
        print('상대방 :', recvData)
        if(len(recvData) == 0): #check connection
            #tm = time.localtime(time.time())
            #timeS = '%d : %d : %d.%d' % (tm.tm_hour, tm.tm_min, tm.tm_sec)
            now = datetime.datetime.now()
            print("연결이 끊어졌습니다. ", now)
            clientSock.close()
            break

        #check link check data
        if(head[4:8] == "0000"):
          sendData = recvData
          sendData = '%04d0000%s' % (len(recvData), recvData)
          print("linke check packet: ", head)
          check_link_time = time.time()
          #time
        #normal packet
        elif(head[4:8] == "2001"):      
          sendData = model.chat(recvData)
          str_len = len(sendData.encode('utf-8'))
          sendData = '%04d2001%s' % (str_len, sendData)
        elif(head[4:8] == "2002"):
          name = recvData[:recvData.find("<///userName///>")].strip()
          query = "select * from user where name='"+name+"'"
          cursor.execute(query)
          result = cursor.fetchall()[0]
          id = result[0]
          prefer = [int(result[2]), int(result[3]), int(result[4]), int(result[5]), int(result[6]), int(result[7])]
          msg = recvData[recvData.find("<///userName///>")+16:].strip()
          
          dic = {'id':id, 'msg': msg, 'pre': prefer }
          q.put(dic)
          print(dic)
          sendData="success"
          print("success")
          
          str_len = len(sendData.encode('utf-8'))
          sendData = '%04d2002%s' % (str_len, sendData)

        #send response packet
        SendChunkData(clientSock, sendData, len(sendData.encode('utf-8')), 60)
        check_link_time = time.time()

        print("나 ", sendData[8:])
    




  conn.close()

from socket import *
import time
import datetime
import select

conn = pymysql.connect(host='lbsg98.duckdns.org', port=20000, user='echatbot', password='echatbot2016', db='echatbotdb', charset='utf8')
cursor = conn.cursor()
q = queue.Queue(32)




if( __name__=="__main__"):
  print("main thread start")

  name = "consumer"
  t = Worker(name)                # sub thread 생성
  t.start()                       # sub thread의 run 메서드를 호출

  socketRun()
