module DiningPhilosophers where

import Control.Monad
import Control.Arrow ((***))
import Control.Concurrent
import Control.Concurrent.STM
import System.Random

import System.IO.Unsafe

printLock :: MVar ()
printLock = unsafePerformIO $ newMVar ()
{-# NOINLINE printLock #-}

atomicPutStrLn = withMVar printLock . const . putStrLn

type Fork = TMVar Int

wait :: IO ()
wait = do
  delay <- randomRIO (0.0, 3.0) :: IO Float
  threadDelay $ truncate $ delay * 1000000.0

acquireFork :: Fork -> IO Int
acquireFork = atomically . takeTMVar

replaceFork :: Fork -> Int -> IO ()
replaceFork fork = atomically . putTMVar fork

think :: String -> IO ()
think name = do
  atomicPutStrLn $ name ++ " is thinking..."
  wait

eat :: String -> IO ()
eat name = do
  atomicPutStrLn $ name ++ " is eating..."
  wait

hungryPhilosopher :: String -> (Fork, Fork) -> IO ()
hungryPhilosopher name forks@(fork1, fork2) = do
  id1 <- acquireFork fork1
  id2 <- acquireFork fork2
  atomicPutStrLn $ name ++ " has acquired forks " ++ show id1 ++ " and " ++ show id2

  eat name

  replaceFork fork2 id2
  replaceFork fork1 id1
  atomicPutStrLn $ name ++ " has replaced forks " ++ show id1 ++ " and " ++ show id2

  philosopher name forks

philosopher :: String -> (Fork, Fork) -> IO ()
philosopher name forks = do
  think name
  hungryPhilosopher name forks

philosophers :: [(String, (Int, Int))]
philosophers = [("Marx", (1, 2)),
                ("Descartes", (2, 3)),
                ("Nietzsche", (3, 4)),
                ("Aristotle", (4, 5)),
                ("Kant", (1, 5))]

main = do
  forks <- mapM newTMVarIO [1..5]
  let getFork id = forks !! (pred id)
      runPhilosopher name = philosopher name . (getFork *** getFork)

  mapM_ (forkIO . uncurry runPhilosopher) philosophers
  getLine
