{-
 Copyright 2019, Mokshasoft AB (mokshasoft.com)

 This software may be distributed and modified according to the terms of
 the GNU General Public License version 3. Note that NO WARRANTY is provided.
 See "LICENSE.txt" for details.
 -}
{-
 Test the libdill channel interface using QuickCheck
 -}
module ChannelQC
  ( quickcheckChannel
  ) where

import qualified Control.Exception as CE
import Control.Monad
import Data.Maybe
import Foreign.C.Types
import Foreign.Storable
import Test.QuickCheck
import Test.QuickCheck.Monadic

import FFI.Channels
import FFI.Handles
import FFI.Helpers
import FFI.TestCaseFFI

-- |Top-level function that runs all libdill channel QuickCheck tests.
quickcheckChannel :: IO ()
quickcheckChannel = do
  quickCheck (withMaxSuccess 10000 prop_GetChannel)
  quickCheck (withMaxSuccess 10000 prop_ReceiverWaitsForSender)
  quickCheck (withMaxSuccess 1000 prop_SimultaneousSenders)
  quickCheck (withMaxSuccess 1000 prop_SimultaneousReceivers)
  quickCheck (withMaxSuccess 100 prop_chdoneUnblocksSenders)

-- |Print a string and trigger an assert
triggerAssert :: String -> IO ()
triggerAssert str = do
  putStrLn str
  CE.assert False $ return ()

-- |Get a channel
getChannel :: IO (CInt, CInt)
getChannel = do
  ch <- dill_chmake
  unless (isJust ch) $ triggerAssert "Failed to get channel"
  return $ fromMaybe (0, 0) ch

-- |Close a channel
closeChannel :: (CInt, CInt) -> IO ()
closeChannel channel = do
  rc1 <- dill_hclose (snd channel)
  unless (rc1 == 0) $ triggerAssert "Failed to close receiver end-point"
  rc2 <- dill_hclose (fst channel)
  unless (rc2 == 0) $ triggerAssert "Failed to close sender end-point"

-- |Close all handles
closeAllHandles :: [CInt] -> IO ()
closeAllHandles handles = do
  rcs <- mapM dill_hclose handles
  unless (all (== 0) rcs) $ triggerAssert "Failed to close all handles"

-- |Test that dill_chmake returns a channel (not really a property test)
prop_GetChannel :: Property
prop_GetChannel =
  monadicIO $ run testProp
  where
    testProp :: IO ()
    testProp = do
      channel <- getChannel
      closeChannel channel

-- |Test that a receiver waits for the sender
prop_ReceiverWaitsForSender :: CInt -> Property
prop_ReceiverWaitsForSender val =
  monadicIO $ do
    res <- run testProp
    assert res
  where
    testProp :: IO Bool
    testProp = do
      channel <- getChannel
      hdl <- ffi_go_sender (fst channel) val
      unless (isJust hdl) $ triggerAssert "Failed to get sender handle"
      let handle = fromMaybe 0 hdl
      rv <- dill_chrecv_int (snd channel)
      unless (isJust rv) $ triggerAssert "Failed to receive value"
      let retVal = fromMaybe 0 rv
      closeChannel channel
      rc <- dill_hclose handle
      unless (rc == 0) $ triggerAssert "Failed to close sender handle"
      return $ val == retVal

-- |Test multiple simultaneous senders, each sender sends one value
prop_SimultaneousSenders :: NonEmptyList CInt -> Property
prop_SimultaneousSenders (NonEmpty vs) =
  monadicIO $ do
    res <- run testProp
    assert res
  where
    testProp :: IO Bool
    testProp = do
      channel <- getChannel
      hdls <- mapM (ffi_go_sender (fst channel)) vs
      unless (all isJust hdls) $
        triggerAssert "Failed to get all sender handles"
      let handles = map (fromMaybe 0) hdls
      rvs <- mapM (\_ -> dill_chrecv_int (snd channel)) handles
      unless (all isJust rvs) $ triggerAssert "Failed to receive all values"
      let retVals = map (fromMaybe 0) rvs
      closeChannel channel
      closeAllHandles handles
      return $ vs == retVals

-- |Test multiple simultaneous receivers, each sender sends one value
prop_SimultaneousReceivers :: NonEmptyList CInt -> Property
prop_SimultaneousReceivers (NonEmpty vs) =
  monadicIO $ do
    res <- run testProp
    assert res
  where
    testProp :: IO Bool
    testProp = do
      channel <- getChannel
      hdls <- mapM (ffi_go_receiver (fst channel)) vs
      unless (all isJust hdls) $
        triggerAssert "Failed to get all receiver handles"
      let handles = map (fromMaybe 0) hdls
      rcs <- mapM (dill_chsend_int (snd channel)) vs
      unless (all (== 0) rcs) $ triggerAssert "Failed to send all values"
      closeChannel channel
      closeAllHandles handles
      return True -- ffi_go_receiver checks that the received values are correct

-- |Test that chdone unblocks all senders
prop_chdoneUnblocksSenders :: NonEmptyList CInt -> Property
prop_chdoneUnblocksSenders (NonEmpty vs) =
  monadicIO $ do
    res <- run testProp
    assert res
  where
    testProp :: IO Bool
    testProp = do
      channel <- getChannel
      hdls <- mapM (ffi_go_sender_unblocked (fst channel)) vs
      unless (all isJust hdls) $
        triggerAssert "Failed to get all sender handles"
      let handles = map (fromMaybe 0) hdls
      now <- dill_now
      rc1 <- dill_msleep $ now + 50
      unless (rc1 == 0) $ triggerAssert "Failed to sleep"
      rc2 <- dill_chdone (snd channel)
      unless (rc2 == 0) $ triggerAssert "Failed to close channel"
      closeChannel channel
      closeAllHandles handles
      return True
