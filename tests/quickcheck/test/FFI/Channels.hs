{-
 Copyright 2019, Mokshasoft AB (mokshasoft.com)

 This software may be distributed and modified according to the terms of
 the GNU General Public License version 3. Note that NO WARRANTY is provided.
 See "LICENSE.txt" for details.
 -}
{-# LANGUAGE ForeignFunctionInterface #-}

{-
 FFI for the channels part of libdill.h
 -}
module FFI.Channels
  ( dill_chmake
  , dill_chsend_int
  , dill_chrecv_int
  , dill_chdone
  ) where

import Control.Monad
import Foreign.C
import Foreign.Marshal.Alloc
import Foreign.Marshal.Array
import Foreign.Ptr
import Foreign.Storable

-- int dill_chmake(int chv[2]);
dill_chmake :: IO (Maybe (CInt, CInt))
dill_chmake = do
  ch <- newArray [0, 0]
  pokeElemOff ch 0 0
  pokeElemOff ch 1 0
  -- Create channel endpoint handles and check that it worked
  res <- internal_dill_chmake ch
  if res /= 0
    then return Nothing
    else do
      ep1 <- peekElemOff ch 0
      ep2 <- peekElemOff ch 1
      if ep1 >= 0 && ep2 >= 0
        then return $ Just (ep1, ep2)
        else return Nothing

foreign import ccall "dill_chmake" internal_dill_chmake :: Ptr CInt -> IO CInt

dill_chsend_int :: CInt -> CInt -> IO CInt
dill_chsend_int ch value = do
  val <- malloc
  pokeElemOff val 0 value
  let valSize = fromIntegral (sizeOf value)
  res <- internal_dill_chsend_int ch val valSize (-1)
  free val
  return res

foreign import ccall "dill_chsend" internal_dill_chsend_int
  :: CInt -> Ptr CInt -> CInt -> CInt -> IO CInt

dill_chrecv_int :: CInt -> IO (Maybe CInt)
dill_chrecv_int ch = do
  val <- malloc
  let valSize = fromIntegral (sizeOf (0 :: CInt))
  res <- internal_dill_chrecv_int ch val valSize (-1)
  ret <-
    if res /= 0
      then return Nothing
      else do
        v <- peekElemOff val 0
        return $ Just v
  free val
  return ret

foreign import ccall "dill_chrecv" internal_dill_chrecv_int
  :: CInt -> Ptr CInt -> CInt -> CInt -> IO CInt

foreign import ccall "dill_chdone" dill_chdone :: CInt -> IO CInt
