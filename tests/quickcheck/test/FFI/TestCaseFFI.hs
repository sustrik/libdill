{-
 Copyright 2019, Mokshasoft AB (mokshasoft.com)

 This software may be distributed and modified according to the terms of
 the GNU General Public License version 3. Note that NO WARRANTY is provided.
 See "LICENSE.txt" for details.
 -}
{-# LANGUAGE ForeignFunctionInterface #-}

{-
 FFI used by the test cases to handle go and coroutine macros
 -}
module FFI.TestCaseFFI
  ( ffi_go_sender
  , ffi_go_sender_unblocked
  , ffi_go_receiver
  ) where

import qualified Control.Exception as CE
import Foreign.C
import Foreign.Ptr

import FFI.Channels

ffi_go_sender :: CInt -> CInt -> IO (Maybe CInt)
ffi_go_sender ch val = do
  hdl <- internal_ffi_go_sender ch val
  if hdl < 0
    then return Nothing
    else return (Just hdl)

foreign import ccall "ffi_go_sender" internal_ffi_go_sender
  :: CInt -> CInt -> IO CInt

ffi_go_sender_unblocked :: CInt -> CInt -> IO (Maybe CInt)
ffi_go_sender_unblocked ch val = do
  hdl <- internal_ffi_go_sender_unblocked ch val
  if hdl < 0
    then return Nothing
    else return (Just hdl)

foreign import ccall "ffi_go_sender_unblocked" internal_ffi_go_sender_unblocked
  :: CInt -> CInt -> IO CInt

ffi_go_receiver :: CInt -> CInt -> IO (Maybe CInt)
ffi_go_receiver ch expected = do
  hdl <- internal_ffi_go_receiver ch expected
  if hdl < 0
    then return Nothing
    else return (Just hdl)

foreign import ccall "ffi_go_receiver" internal_ffi_go_receiver
  :: CInt -> CInt -> IO CInt
